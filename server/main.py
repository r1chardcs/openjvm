import base64
import binascii
import io
import json
import struct
import zipfile
from pathlib import Path
from typing import Any, List, Optional

import uvicorn
from fastapi import Depends, FastAPI, HTTPException
from fastapi.responses import HTMLResponse
from pydantic import BaseModel, Field
from sqlalchemy import Column, Integer, String, Text, create_engine
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import Session, sessionmaker


app = FastAPI(title="OpenJVM Server", version="1.1.0")

DATABASE_URL = "sqlite:///./scripts.db"
engine = create_engine(DATABASE_URL, connect_args={"check_same_thread": False})
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()

MAX_JAR_SIZE = 64 * 1024 * 1024
MAX_FILES = 50
MATCH_THRESHOLD = 0.30


class ScriptDB(Base):
    __tablename__ = "scripts"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String, unique=True, nullable=False)
    ring = Column(Integer, nullable=False)
    code = Column(Text, nullable=False)


Base.metadata.create_all(bind=engine)

class Script(BaseModel):
    id: Optional[int] = None
    name: str
    ring: int
    code: str

    class Config:
        from_attributes = True


class JarFilePayload(BaseModel):
    name: str = Field(min_length=1, max_length=255)
    data: str = Field(min_length=1)


class JarImportPayload(BaseModel):
    name: str = Field(min_length=1, max_length=255)
    ring: int
    files: List[JarFilePayload] = Field(min_length=1, max_length=MAX_FILES)


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


class ClassReader:
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0

    def take(self, size: int) -> bytes:
        end = self.offset + size
        if end > len(self.data):
            raise ValueError("Unexpected end of class file")
        value = self.data[self.offset:end]
        self.offset = end
        return value

    def u1(self) -> int:
        return self.take(1)[0]

    def u2(self) -> int:
        return struct.unpack(">H", self.take(2))[0]

    def u4(self) -> int:
        return struct.unpack(">I", self.take(4))[0]


def parse_class_file(data: bytes) -> dict[str, Any]:
    reader = ClassReader(data)
    if reader.u4() != 0xCAFEBABE:
        raise ValueError("Not a Java class")
    reader.take(4)  # minor + major version

    constant_pool_count = reader.u2()
    constant_pool: list[Any] = [None] * constant_pool_count
    index = 1
    while index < constant_pool_count:
        tag = reader.u1()
        if tag == 1:
            size = reader.u2()
            constant_pool[index] = ("utf8", reader.take(size).decode("utf-8", "replace"))
        elif tag in (3, 4):
            reader.take(4)
        elif tag in (5, 6):
            reader.take(8)
            index += 1
        elif tag in (7, 8, 16, 19, 20):
            constant_pool[index] = (tag, reader.u2())
        elif tag in (9, 10, 11, 12, 17, 18):
            constant_pool[index] = (tag, reader.u2(), reader.u2())
        elif tag == 15:
            reader.take(3)
        else:
            raise ValueError(f"Unsupported constant-pool tag: {tag}")
        index += 1

    def utf8(cp_index: int) -> str:
        if not cp_index or cp_index >= len(constant_pool):
            return ""
        entry = constant_pool[cp_index]
        return entry[1] if entry and entry[0] == "utf8" else ""

    def class_name(cp_index: int) -> str:
        if not cp_index or cp_index >= len(constant_pool):
            return ""
        entry = constant_pool[cp_index]
        if not entry or entry[0] != 7:
            return ""
        return utf8(entry[1]).replace("/", ".")

    reader.u2()  # access flags
    this_class = reader.u2()
    super_class = reader.u2()
    interfaces = [class_name(reader.u2()) for _ in range(reader.u2())]

    def read_members() -> list[tuple[str, str]]:
        members: list[tuple[str, str]] = []
        for _ in range(reader.u2()):
            reader.u2()
            member_name = utf8(reader.u2())
            descriptor = utf8(reader.u2())
            members.append((member_name, descriptor))
            for _ in range(reader.u2()):
                reader.u2()
                reader.take(reader.u4())
        return members

    fields = read_members()
    methods = read_members()

    name = class_name(this_class)
    return {
        "name": name,
        "simple": name.rsplit(".", 1)[-1],
        "super": class_name(super_class),
        "interfaces": sorted(filter(None, interfaces))[:16],
        "methods": sorted(
            f"{method_name}{descriptor}"
            for method_name, descriptor in methods
            if method_name not in {"<init>", "<clinit>"}
        )[:40],
        "method_names": sorted(
            {method_name for method_name, _ in methods if method_name not in {"<init>", "<clinit>"}}
        )[:40],
        "fields": sorted(f"{field_name}:{descriptor}" for field_name, descriptor in fields)[:30],
        "field_names": sorted({field_name for field_name, _ in fields})[:30],
    }


def fingerprints_from_jars(files: List[JarFilePayload]) -> tuple[list[dict[str, Any]], list[str]]:
    fingerprints: list[dict[str, Any]] = []
    warnings: list[str] = []
    seen: set[tuple[Any, ...]] = set()

    for uploaded in files:
        if not uploaded.name.lower().endswith(".jar"):
            raise HTTPException(status_code=400, detail=f"{uploaded.name}: expected a .jar file")
        try:
            encoded = uploaded.data.split(",", 1)[-1]
            jar_data = base64.b64decode(encoded, validate=True)
        except (ValueError, binascii.Error):
            raise HTTPException(status_code=400, detail=f"{uploaded.name}: invalid base64 data")
        if len(jar_data) > MAX_JAR_SIZE:
            raise HTTPException(status_code=413, detail=f"{uploaded.name}: file is larger than 64 MB")

        try:
            with zipfile.ZipFile(io.BytesIO(jar_data)) as archive:
                class_entries = [
                    item for item in archive.infolist()
                    if item.filename.endswith(".class")
                    and not item.is_dir()
                    and not item.filename.startswith("META-INF/versions/")
                ]
                if not class_entries:
                    warnings.append(f"{uploaded.name}: no .class files found")
                for item in class_entries:
                    if item.file_size > 16 * 1024 * 1024:
                        warnings.append(f"{uploaded.name}: skipped oversized {item.filename}")
                        continue
                    try:
                        fingerprint = parse_class_file(archive.read(item))
                    except (ValueError, EOFError, struct.error, zipfile.BadZipFile):
                        warnings.append(f"{uploaded.name}: could not read {item.filename}")
                        continue
                    key = (
                        fingerprint["name"],
                        tuple(fingerprint["methods"]),
                        tuple(fingerprint["fields"]),
                    )
                    if fingerprint["name"] and key not in seen:
                        seen.add(key)
                        fingerprints.append(fingerprint)
        except zipfile.BadZipFile:
            raise HTTPException(status_code=400, detail=f"{uploaded.name}: invalid or damaged JAR")

    if not fingerprints:
        raise HTTPException(status_code=400, detail="No readable Java classes were found")
    return fingerprints, warnings


def generate_checker(fingerprints: list[dict[str, Any]], sources: list[str]) -> str:
    targets = json.dumps(fingerprints, ensure_ascii=False, separators=(",", ":"))
    source_names = ", ".join(sources)
    return f'''"""Generated from: {source_names}
Matches a class when at least 30% of the available weighted features agree.
"""

MATCH_THRESHOLD = {MATCH_THRESHOLD}
TARGETS = {targets}


def _text(value):
    return "" if value is None else str(value).replace("/", ".")


def _values(value, name_keys=("name",), descriptor_keys=("descriptor", "desc", "signature")):
    if value is None:
        return set(), set()
    if isinstance(value, dict):
        value = list(value.values())
    if not isinstance(value, (list, tuple, set)):
        value = [value]
    full, names = set(), set()
    for item in value:
        if isinstance(item, dict):
            name = next((_text(item.get(key)) for key in name_keys if item.get(key) is not None), "")
            descriptor = next(
                (_text(item.get(key)) for key in descriptor_keys if item.get(key) is not None), ""
            )
            if name:
                names.add(name)
                full.add(name + descriptor if descriptor else name)
        else:
            text = _text(item)
            if text:
                full.add(text)
                names.add(text.split("(", 1)[0].split(":", 1)[0])
    return full, names


def _first(header, *keys):
    for key in keys:
        value = header.get(key)
        if value is not None:
            return value
    return None


def _score(header, target):
    name = _text(_first(header, "name", "class_name", "className", "internalName"))
    simple = name.rsplit(".", 1)[-1] if name else ""
    super_name = _text(_first(header, "super", "super_name", "superName", "parent"))
    interfaces, _ = _values(_first(header, "interfaces", "interface_names", "interfaceNames"))
    methods, method_names = _values(_first(header, "methods", "method_list", "methodList"))
    fields, field_names = _values(_first(header, "fields", "field_list", "fieldList"))

    points = 0.0
    possible = 0.0

    if name:
        possible += 6.0
        if name == target["name"]:
            points += 6.0
        elif simple == target["simple"]:
            points += 3.0
    if super_name and target["super"]:
        possible += 2.0
        if super_name == target["super"]:
            points += 2.0
    if interfaces and target["interfaces"]:
        weight = min(len(target["interfaces"]), 4)
        possible += float(weight)
        points += float(min(len(interfaces.intersection(target["interfaces"])), weight))
    if methods and target["methods"]:
        weight = min(len(target["methods"]), 12)
        possible += float(weight)
        exact = len(methods.intersection(target["methods"]))
        by_name = len(method_names.intersection(target["method_names"]))
        points += float(min(exact + max(0, by_name - exact) * 0.5, weight))
    if fields and target["fields"]:
        weight = min(len(target["fields"]), 8)
        possible += float(weight)
        exact = len(fields.intersection(target["fields"]))
        by_name = len(field_names.intersection(target["field_names"]))
        points += float(min(exact + max(0, by_name - exact) * 0.5, weight))

    return points / possible if possible else 0.0


def check(header):
    """Return True if the supplied class header matches any reference class by >= 30%."""
    if not isinstance(header, dict):
        return False
    return any(_score(header, target) >= MATCH_THRESHOLD for target in TARGETS)
'''


@app.get("/api/script/get/{id}")
async def getScripts(id: int, db: Session = Depends(get_db)):
    script = db.query(ScriptDB).filter(ScriptDB.id == id).first()
    if not script:
        raise HTTPException(status_code=404, detail="Script not found")
    return Script.model_validate(script)


@app.get("/admin", response_class=HTMLResponse)
async def admin_panel():
    return ADMIN_HTML


@app.get("/api/scripts", response_model=List[Script])
async def get_all_scripts(db: Session = Depends(get_db)):
    scripts = db.query(ScriptDB).order_by(ScriptDB.id.desc()).all()
    return [Script.model_validate(s) for s in scripts]


@app.post("/api/scripts", response_model=Script)
async def create_script(script: Script, db: Session = Depends(get_db)):
    existing = db.query(ScriptDB).filter(ScriptDB.name == script.name).first()
    if existing:
        raise HTTPException(status_code=400, detail="Script with this name already exists")
    db_script = ScriptDB(name=script.name, ring=script.ring, code=script.code)
    db.add(db_script)
    db.commit()
    db.refresh(db_script)
    return Script.model_validate(db_script)


@app.post("/api/scripts/import-jars", response_model=Script)
async def import_jars(payload: JarImportPayload, db: Session = Depends(get_db)):
    existing = db.query(ScriptDB).filter(ScriptDB.name == payload.name).first()
    if existing:
        raise HTTPException(status_code=400, detail="Script with this name already exists")

    fingerprints, warnings = fingerprints_from_jars(payload.files)
    code = generate_checker(fingerprints, [file.name for file in payload.files])
    if warnings:
        code = "# Import warnings: " + " | ".join(warnings[:20]) + "\n" + code

    db_script = ScriptDB(name=payload.name, ring=payload.ring, code=code)
    db.add(db_script)
    db.commit()
    db.refresh(db_script)
    return Script.model_validate(db_script)


@app.delete("/api/scripts/{id}")
async def delete_script(id: int, db: Session = Depends(get_db)):
    script = db.query(ScriptDB).filter(ScriptDB.id == id).first()
    if not script:
        raise HTTPException(status_code=404, detail="Script not found")
    db.delete(script)
    db.commit()
    return {"message": "Script deleted"}


ADMIN_HTML = r"""
<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>OpenJVM · Admin</title>
  <style>
    :root{color-scheme:dark;--bg:#090c12;--panel:#111722;--panel2:#171f2d;--line:#283247;
      --text:#f4f7fb;--muted:#8d9ab0;--blue:#6c8cff;--blue2:#4e70ef;--red:#ff6577;--green:#4bd6a1}
    *{box-sizing:border-box} body{margin:0;background:
      radial-gradient(circle at 15% -10%,#24345b 0,transparent 34%),var(--bg);
      color:var(--text);font-family:Inter,ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif}
    button,input,textarea{font:inherit}.shell{max-width:1180px;margin:auto;padding:34px 22px 60px}
    header{display:flex;align-items:center;justify-content:space-between;gap:18px;margin-bottom:30px}
    .brand{display:flex;align-items:center;gap:13px}.logo{display:grid;place-items:center;width:45px;height:45px;
      border-radius:14px;background:linear-gradient(145deg,var(--blue),#9b6cff);box-shadow:0 10px 35px #6c8cff45;
      font-weight:900;font-size:20px}.brand h1{font-size:20px;margin:0}.brand p{margin:3px 0 0;color:var(--muted);font-size:13px}
    .status{display:flex;align-items:center;gap:8px;color:var(--muted);font-size:13px}.dot{width:8px;height:8px;
      border-radius:50%;background:var(--green);box-shadow:0 0 12px var(--green)}
    .grid{display:grid;grid-template-columns:minmax(300px,420px) 1fr;gap:22px}
    .card{background:#111722db;border:1px solid var(--line);border-radius:20px;box-shadow:0 20px 60px #0005;
      overflow:hidden}.card-head{padding:21px 22px;border-bottom:1px solid var(--line)}
    .card-head h2{font-size:16px;margin:0}.card-head p{color:var(--muted);font-size:13px;margin:6px 0 0;line-height:1.5}
    .form{padding:22px}.field{margin-bottom:17px}.field label{display:block;font-size:12px;color:#b6c0d1;
      margin:0 0 8px;font-weight:650}.row{display:grid;grid-template-columns:1fr 105px;gap:11px}
    input,textarea{width:100%;border:1px solid var(--line);background:#0c111a;color:var(--text);border-radius:11px;
      padding:11px 12px;outline:none;transition:.2s}input:focus,textarea:focus{border-color:var(--blue);
      box-shadow:0 0 0 3px #6c8cff20}textarea{min-height:145px;resize:vertical;font-family:ui-monospace,monospace;font-size:12px}
    .drop{border:1.5px dashed #3b4862;border-radius:15px;padding:24px 15px;text-align:center;cursor:pointer;
      background:#0c111a;transition:.2s}.drop:hover,.drop.drag{border-color:var(--blue);background:#121a2b}
    .drop b{display:block;font-size:14px}.drop span{display:block;color:var(--muted);font-size:12px;margin-top:6px}
    .files{display:flex;flex-wrap:wrap;gap:7px;margin-top:10px}.chip{max-width:100%;display:flex;gap:7px;align-items:center;
      background:#1b2536;border:1px solid #303c52;border-radius:9px;padding:6px 8px;font-size:11px;color:#c7d0df}
    .chip span{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.chip button{border:0;background:none;color:var(--muted);
      cursor:pointer;padding:0}.btn{width:100%;border:0;border-radius:11px;padding:12px 15px;color:white;font-weight:700;
      cursor:pointer;background:linear-gradient(135deg,var(--blue),var(--blue2));box-shadow:0 8px 22px #4e70ef33}
    .btn:disabled{opacity:.55;cursor:wait}.tabs{display:flex;gap:6px;margin-bottom:17px}.tab{flex:1;border:1px solid var(--line);
      border-radius:10px;background:#0c111a;color:var(--muted);padding:9px;cursor:pointer}.tab.active{color:white;border-color:#51699e;
      background:#1a2540}.hidden{display:none}.list-head{display:flex;align-items:center;justify-content:space-between}
    .count{color:var(--muted);font-size:12px}.list{padding:10px;max-height:675px;overflow:auto}
    .empty{text-align:center;padding:80px 20px;color:var(--muted)}.item{display:grid;grid-template-columns:42px 1fr auto;
      gap:13px;align-items:center;padding:13px;border-radius:13px;border:1px solid transparent}.item:hover{background:#161e2b;
      border-color:#263147}.file-icon{width:42px;height:42px;border-radius:11px;display:grid;place-items:center;
      background:#202b43;color:#8fa7ff;font-size:11px;font-weight:800}.meta{min-width:0}.meta b{display:block;overflow:hidden;
      text-overflow:ellipsis;white-space:nowrap;font-size:14px}.meta span{color:var(--muted);font-size:11px}
    .actions{display:flex;gap:5px}.icon-btn{border:1px solid var(--line);background:#0c111a;color:#aeb9ca;border-radius:9px;
      padding:7px 9px;cursor:pointer}.icon-btn:hover{color:white;border-color:#52617a}.icon-btn.danger:hover{color:var(--red);
      border-color:#743542}.modal{position:fixed;inset:0;background:#05070bbd;backdrop-filter:blur(8px);display:none;
      align-items:center;justify-content:center;padding:22px;z-index:10}.modal.show{display:flex}.modal-card{width:min(880px,100%);
      max-height:88vh;background:var(--panel);border:1px solid var(--line);border-radius:19px;overflow:hidden}
    .modal-head{display:flex;justify-content:space-between;align-items:center;padding:15px 18px;border-bottom:1px solid var(--line)}
    .modal-head b{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.modal-actions{display:flex;gap:6px}
    pre{margin:0;padding:20px;overflow:auto;max-height:70vh;background:#090d14;color:#cbd7ea;font-size:12px;line-height:1.55}
    .toast{position:fixed;right:22px;bottom:22px;max-width:360px;padding:12px 15px;border-radius:11px;
      background:#1d2939;border:1px solid #39485e;box-shadow:0 15px 40px #0008;transform:translateY(90px);
      opacity:0;transition:.25s;font-size:13px;z-index:20}.toast.show{transform:none;opacity:1}.toast.error{border-color:#7a3540}
    @media(max-width:820px){.grid{grid-template-columns:1fr}.list{max-height:none}header{align-items:flex-start}.status{display:none}}
  </style>
</head>
<body>
<main class="shell">
  <header>
    <div class="brand"><div class="logo">J</div><div><h1>OpenJVM Admin</h1><p>Управление Python-проверками классов</p></div></div>
    <div class="status"><i class="dot"></i> API работает</div>
  </header>
  <div class="grid">
    <section class="card">
      <div class="card-head"><h2>Новая проверка</h2><p>Загрузите один или несколько JAR — панель создаст функцию <code>check(header)</code>.</p></div>
      <div class="form">
        <div class="tabs"><button class="tab active" data-tab="jar">Импорт JAR</button><button class="tab" data-tab="manual">Вручную</button></div>
        <div class="row">
          <div class="field"><label for="name">Название</label><input id="name" maxlength="255" placeholder="Например, Combat mod"></div>
          <div class="field"><label for="ring">Ring</label><input id="ring" type="number" value="0"></div>
        </div>
        <div id="jarPane">
          <input id="picker" class="hidden" type="file" accept=".jar,application/java-archive" multiple>
          <div id="drop" class="drop"><b>Перетащите .jar сюда</b><span>или нажмите, чтобы выбрать · до 50 файлов</span></div>
          <div id="files" class="files"></div>
          <p style="font-size:11px;color:var(--muted);line-height:1.5;margin:13px 2px 17px">Совпадение считается по имени, родителю, интерфейсам, методам и полям. Порог — 30% доступных баллов.</p>
          <button id="generate" class="btn">Сгенерировать и сохранить</button>
        </div>
        <div id="manualPane" class="hidden">
          <div class="field"><label for="code">Python-код</label><textarea id="code" spellcheck="false" placeholder="def check(header):&#10;    return False"></textarea></div>
          <button id="saveManual" class="btn">Сохранить скрипт</button>
        </div>
      </div>
    </section>
    <section class="card">
      <div class="card-head list-head"><div><h2>Скрипты</h2><p>Созданные и импортированные проверки</p></div><span id="count" class="count">0</span></div>
      <div id="list" class="list"><div class="empty">Загрузка…</div></div>
    </section>
  </div>
</main>
<div id="modal" class="modal"><div class="modal-card">
  <div class="modal-head"><b id="modalTitle">Скрипт</b><div class="modal-actions">
    <button id="copy" class="icon-btn">Копировать</button><button id="download" class="icon-btn">Скачать .py</button><button id="close" class="icon-btn">✕</button>
  </div></div><pre><code id="viewer"></code></pre>
</div></div>
<div id="toast" class="toast"></div>
<script>
  const $ = id => document.getElementById(id);
  let selected = [], scripts = [], opened = null;
  const escapeHtml = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  function toast(message, error=false){const el=$('toast');el.textContent=message;el.className='toast show'+(error?' error':'');clearTimeout(el.timer);el.timer=setTimeout(()=>el.className='toast',3200)}
  async function api(url, options){const res=await fetch(url,options);let data=null;try{data=await res.json()}catch{}if(!res.ok)throw new Error(data?.detail||`Ошибка ${res.status}`);return data}
  function renderFiles(){ $('files').innerHTML=selected.map((f,i)=>`<div class="chip"><span>${escapeHtml(f.name)} · ${(f.size/1048576).toFixed(1)} MB</span><button data-remove="${i}">✕</button></div>`).join('') }
  function addFiles(files){const valid=[...files].filter(f=>f.name.toLowerCase().endsWith('.jar')); if(valid.length!==files.length)toast('Разрешены только .jar файлы',true);selected=[...selected,...valid].slice(0,50);renderFiles()}
  $('drop').onclick=()=>$('picker').click();$('picker').onchange=e=>addFiles(e.target.files);
  for(const type of ['dragenter','dragover'])$('drop').addEventListener(type,e=>{e.preventDefault();$('drop').classList.add('drag')});
  for(const type of ['dragleave','drop'])$('drop').addEventListener(type,e=>{e.preventDefault();$('drop').classList.remove('drag')});
  $('drop').addEventListener('drop',e=>addFiles(e.dataTransfer.files));
  $('files').onclick=e=>{const i=e.target.dataset.remove;if(i!==undefined){selected.splice(Number(i),1);renderFiles()}};
  document.querySelectorAll('.tab').forEach(tab=>tab.onclick=()=>{document.querySelectorAll('.tab').forEach(x=>x.classList.toggle('active',x===tab));$('jarPane').classList.toggle('hidden',tab.dataset.tab!=='jar');$('manualPane').classList.toggle('hidden',tab.dataset.tab!=='manual')});
  function readBase64(file){return new Promise((resolve,reject)=>{const r=new FileReader();r.onload=()=>resolve(String(r.result).split(',')[1]);r.onerror=reject;r.readAsDataURL(file)})}
  async function load(){try{scripts=await api('/api/scripts');$('count').textContent=`${scripts.length} шт.`;$('list').innerHTML=scripts.length?scripts.map(s=>`
    <div class="item"><div class="file-icon">PY</div><div class="meta"><b>${escapeHtml(s.name)}</b><span>ID ${s.id} · Ring ${s.ring} · ${(s.code.length/1024).toFixed(1)} KB</span></div>
    <div class="actions"><button class="icon-btn" data-view="${s.id}" title="Открыть">⌘</button><button class="icon-btn danger" data-delete="${s.id}" title="Удалить">✕</button></div></div>`).join(''):'<div class="empty">Скриптов пока нет</div>'}catch(e){$('list').innerHTML='<div class="empty">Не удалось загрузить список</div>';toast(e.message,true)}}
  function formValues(){const name=$('name').value.trim(),ring=Number.parseInt($('ring').value,10);if(!name)throw new Error('Укажите название');if(!Number.isInteger(ring))throw new Error('Ring должен быть целым числом');return{name,ring}}
  $('generate').onclick=async()=>{let values;try{values=formValues();if(!selected.length)throw new Error('Выберите хотя бы один JAR');$('generate').disabled=true;$('generate').textContent='Чтение JAR…';const files=await Promise.all(selected.map(async f=>({name:f.name,data:await readBase64(f)})));$('generate').textContent='Анализ классов…';const created=await api('/api/scripts/import-jars',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({...values,files})});selected=[];renderFiles();$('name').value='';toast(`Готово: ${created.name}`);await load();openScript(created)}catch(e){toast(e.message,true)}finally{$('generate').disabled=false;$('generate').textContent='Сгенерировать и сохранить'}};
  $('saveManual').onclick=async()=>{try{const values=formValues(),code=$('code').value;if(!code.trim())throw new Error('Введите Python-код');$('saveManual').disabled=true;const created=await api('/api/scripts',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({...values,code})});$('name').value='';$('code').value='';toast('Скрипт сохранён');await load();openScript(created)}catch(e){toast(e.message,true)}finally{$('saveManual').disabled=false}};
  $('list').onclick=async e=>{const view=e.target.dataset.view,del=e.target.dataset.delete;if(view){const script=scripts.find(s=>s.id===Number(view));if(script)openScript(script)}if(del&&confirm('Удалить этот скрипт?')){try{await api(`/api/scripts/${del}`,{method:'DELETE'});toast('Скрипт удалён');load()}catch(err){toast(err.message,true)}}};
  function openScript(script){opened=script;$('modalTitle').textContent=`${script.name} · Ring ${script.ring}`;$('viewer').textContent=script.code;$('modal').classList.add('show')}
  function closeModal(){$('modal').classList.remove('show')} $('close').onclick=closeModal;$('modal').onclick=e=>{if(e.target===$('modal'))closeModal()};
  $('copy').onclick=async()=>{if(opened){await navigator.clipboard.writeText(opened.code);toast('Код скопирован')}};
  $('download').onclick=()=>{if(!opened)return;const blob=new Blob([opened.code],{type:'text/x-python'}),a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download=(opened.name.replace(/[^a-zа-яё0-9_-]+/gi,'_')||'checker')+'.py';a.click();URL.revokeObjectURL(a.href)};
  document.addEventListener('keydown',e=>{if(e.key==='Escape')closeModal()});load();
</script>
</body>
</html>
"""


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
