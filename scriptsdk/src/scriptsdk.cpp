#include <cstdio>
#include <scriptsdk.h>

#include "common_exception.h"
#include "common_memory.h"
#include "py/pocketpy.h"

#include "nlohmann/json.hpp"
#include "HTTPRequest/HTTPRequest.hpp"

void DestroyTargetScript(PTargetScript Script) {
    if (!Script)
        return;

    CommonFree(PV(Script->name));
    CommonFree(PV(Script->source));
    CommonFree(Script);
    Script = nullptr;
}

PTargetScript CreateTargetScript(I64 Ring, CPI1 Name, CPI1 Source, CheckingType Type) {
    const auto script
        = static_cast<PTargetScript>(CommonMalloc(sizeof(TargetScript)));

    script->name = StrDup(Name);
    script->ring = Ring;
    script->source = StrDup(Source);
    script->type = Type;

    return script;
}

TargetExecuteResult ExecuteTargetScript(TargetScript Script, TargetClassHeader Header) {
    py_exec(Script.source, "<script_string>", EXEC_MODE, nullptr);

    const auto func_name = py_name("check");
    if (!func_name) {
        CommonThrow("Invalid-Code: Not found name 'check'.");
        py_finalize();
        return {false, nullptr};
    }

    const auto func_ref = py_getglobal(func_name);
    if (!func_ref) {
        CommonThrow("Invalid-Code: Not found Reference 'check'.");
        py_finalize();
        return {false, nullptr};
    }

    py_push(static_cast<py_Ref>(func_ref));
    py_pushnil();

    py_newdict(py_r0());

    py_newstr(py_r1(), Header.Name);
    py_dict_setitem_by_str(py_r0(), "name", py_r1());

    py_newint(py_r1(), Header.Size);
    py_dict_setitem_by_str(py_r0(), "size", py_r1());

    py_newlist(py_r1());
    for (U64 i = 0; i < Header.MethodsSize; i++) {
        py_newdict(py_r2());

        py_newstr(py_r3(), Header.Methods[i].Name);
        py_dict_setitem_by_str(py_r2(), "name", py_r3());

        py_newstr(py_r3(), Header.Methods[i].Signature);
        py_dict_setitem_by_str(py_r2(), "signature", py_r3());

        py_list_append(py_r1(), py_r2());
    }
    py_dict_setitem_by_str(py_r0(), "methods", py_r1());

    py_newlist(py_r1());
    for (U64 i = 0; i < Header.FieldsSize; i++) {
        py_newdict(py_r2());

        py_newstr(py_r3(), Header.Fields[i].Name);
        py_dict_setitem_by_str(py_r2(), "name", py_r3());

        py_newstr(py_r3(), Header.Fields[i].Signature);
        py_dict_setitem_by_str(py_r2(), "signature", py_r3());

        py_list_append(py_r1(), py_r2());
    }
    py_dict_setitem_by_str(py_r0(), "fields", py_r1());

    py_push(py_r0());
    py_vectorcall(1, 0);

    BOOL result = py_tobool(py_retval());

    CPI1 verbose = nullptr;
    py_ItemRef verbose_ref = py_getglobal(py_name("Out_Verbose"));
    if (verbose_ref && py_isstr(verbose_ref)) {
        verbose = py_tostr(verbose_ref);
    }

    return {result, verbose};
}

PTargetServerScript GetScriptByID(I64 id, CPI1 address) {
    try {
        char url[512];
        sprintf(url, "%s/api/script/get/%lld", address, id);

        http::Request request(url);
        const auto result = request.send("GET", "", {}, std::chrono::milliseconds{500});

        if (result.status.code == 404) {
            return nullptr;
        }

        if (result.status.code != 200) {
            return nullptr;
        }

        auto body = result.body;
        std::string jsonStr(body.begin(), body.end());

        auto json = nlohmann::json::parse(jsonStr);

        auto script = static_cast<PTargetServerScript>(CommonMalloc(sizeof(TargetServerScript)));
        if (!script) {
            return nullptr;
        }

        script->Name = CommonStrDup(json["name"].get<std::string>().c_str());
        script->Source = CommonStrDup(json["code"].get<std::string>().c_str());
        script->Ring = json["ring"].get<I64>();

        return script;
    }
    catch (...) {
        return nullptr;
    }
}

PTargetScript ToTargetScript(PTargetServerScript Script) {
    auto out = CreateTargetScript(Script->Ring, Script->Name, Script->Source, CHECKING_TYPE_HEADER);
    CommonFree((void*)Script->Name);
    CommonFree((void*)Script->Source);
    CommonFree(Script);
    return out;
}

void InitializeScriptSDK() {
    py_initialize();
}

void ShutdownScriptSDK() {
    py_finalize();
}
