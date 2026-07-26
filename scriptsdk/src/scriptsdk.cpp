#include <cstdio>
#include <scriptsdk.h>

#include "common_exception.h"
#include "common_memory.h"
#include "py/pocketpy.h"

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

BOOL ExecuteTargetScript(TargetScript Script, TargetClassHeader Header) {
    py_exec(Script.source, "<script_string>", EXEC_MODE, nullptr);

    const auto func_name = py_name("check");
    if (!func_name) {
        CommonThrow("Invalid-Code: Not found name 'check'.");
        py_finalize();
        return false;
    }

    const auto func_ref = py_getglobal(func_name);
    if (!func_ref) {
        CommonThrow("Invalid-Code: Not found Reference 'check'.");
        py_finalize();
        return false;
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

    return result;
}
