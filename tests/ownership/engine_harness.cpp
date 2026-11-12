/*
 * Synthetic 4D engine boundary for ownership regressions.
 *
 * Compiles the real plugin entry point (4DPlugin-Universal-Document-Parser.cpp,
 * included below so its static helpers are reachable) and links the real
 * support/4DPlugin-JSON.cpp, C_TEXT.cpp and the bundled, unmodified
 * 4D Plugin API (4DPluginAPI.c). Only gCall4D is replaced: it models the
 * engine-side allocations (Unistrings, pictures, objects/collections,
 * handles) with malloc/free so ASan reports double frees / use-after-free,
 * and keeps a live set of every engine resource so leaks are detected exactly.
 * Extractor libraries and the llama.cpp tokenizer are stubbed in
 * boundary_stubs.cpp.
 *
 * Engine semantics modelled (per the SDK contract):
 *  - PA_CreateVariable(eVK_Unistring/eVK_Picture/eVK_Blob) allocates an empty
 *    value.
 *  - PA_SetObjectProperty/PA_SetCollectionElement deep-copy strings and
 *    pictures and retain objects/collections; the caller still owns its
 *    variable.
 *  - PA_GetObjectProperty returns an owned deep copy or a retained
 *    object/collection that must be cleared; on error the returned variable
 *    is left uninitialised (poisoned here, see g_fail_key).
 *  - PA_ExecuteCommandByID returns an owned result (New Object, 4D.File, ...).
 *  - PA_ClearVariable releases what the variable holds, including an object
 *    reference the plugin merely borrowed.
 */

/* Resolved through -I <source-root>, so the same harness tests baseline and fixed trees. */
#include "4DPlugin-Universal-Document-Parser.cpp"

#include "PrivateTypes.h"
#include "EntryPoints.h"
#include "boundary_stubs.h"

#include <map>
#include <set>
#include <unistd.h>

extern "C" Call4DProcPtr gCall4D;

namespace {

typedef std::basic_string<PA_Unichar> u16;

struct Obj {
    int rc;
    bool coll;
    std::map<u16, PA_Variable> props;
    std::vector<PA_Variable> items;
    std::string bytes; /* contents when the object stands for a 4D.Blob */
};

struct Handle { char *data; size_t size; };

std::set<void *> g_strings;
std::set<void *> g_pictures;
std::set<Obj *> g_objects;
std::set<void *> g_handles;
std::set<Obj *> g_blob_instances;
Obj *g_4d = nullptr;         /* the "4D" global object (command 1709) */
Obj *g_blob_class = nullptr; /* 4D.Blob */
int g_file_objects = 0;      /* results of 4D.File / 4D.Folder (1566/1567) */

/* PA_GetObjectProperty for this key fails: eb.fError is set and the result
   is left uninitialised. The poison stands in for that garbage: readable
   (so tests fail by assertion, not by crashing) but never engine-owned, so
   clearing it is counted rather than freed. */
std::string g_fail_key;
bool g_poison_real = false; /* poison as eVK_Real 3.0 instead of text */
const char kPoisonMark = 0x5A; /* fFiller of a poisoned result */
int g_failed_gets = 0;
int g_poison_clears = 0;
PA_Unichar kPoisonBuf[] = { 'B', 'A', 'D', '!', 0 };
PA_Unichar *const kPoison = kPoisonBuf;

[[noreturn]] void die(const char *msg) {
    fprintf(stderr, "ENGINE VIOLATION: %s\n", msg);
    fflush(stderr);
    abort();
}

PA_Unistring new_string(const PA_Unichar *src, size_t len) {
    PA_Unichar *buf = (PA_Unichar *)malloc((len + 1) * sizeof(PA_Unichar));
    if(len) memcpy(buf, src, len * sizeof(PA_Unichar));
    buf[len] = 0;
    g_strings.insert(buf);
    PA_Unistring s;
    s.fLength = (PA_long32)len;
    s.fString = buf;
    s.fReserved1 = 0;
    s.fReserved2 = 0;
    return s;
}

void free_string(PA_Unistring *s) {
    if(!s->fString) return;
    if(s->fString == kPoison) {
        g_poison_clears++;
        fprintf(stderr, "  cleared the uninitialised result of a failed PA_GetObjectProperty\n");
        s->fString = 0;
        return;
    }
    if(!g_strings.erase(s->fString)) die("disposed a string the engine does not own");
    free(s->fString);
    s->fString = 0;
    s->fLength = 0;
}

void *new_picture() {
    void *p = malloc(16);
    g_pictures.insert(p);
    return p;
}

void free_picture(void *p) {
    if(!p) return;
    if(!g_pictures.erase(p)) die("disposed a picture the engine does not own");
    free(p);
}

Handle *new_handle(size_t size) {
    Handle *h = (Handle *)malloc(sizeof(Handle));
    h->size = size;
    h->data = (char *)malloc(size ? size : 1);
    g_handles.insert(h);
    return h;
}

void free_handle(Handle *h) {
    if(!h) return;
    if(!g_handles.erase(h)) die("disposed an unknown handle");
    free(h->data);
    free(h);
}

Handle *live_handle(void *p) {
    if(!g_handles.count(p)) die("used an unknown handle");
    return (Handle *)p;
}

void release_object(Obj *o);

void clear_var(PA_Variable *v) {
    if(v->fFiller == kPoisonMark) {
        g_poison_clears++;
        fprintf(stderr, "  cleared the uninitialised result of a failed PA_GetObjectProperty\n");
        v->fType = eVK_Undefined;
        v->fFiller = 0;
        return;
    }
    switch(v->fType) {
        case eVK_Unistring: free_string(&v->uValue.fString); break;
        case eVK_Picture: free_picture(v->uValue.fPicture); break;
        case eVK_Blob: free_handle((Handle *)v->uValue.fBlob.fHandle); break;
        case eVK_Object: if(v->uValue.fObject) release_object((Obj *)v->uValue.fObject); break;
        case eVK_Collection: if(v->uValue.fCollection) release_object((Obj *)v->uValue.fCollection); break;
        default: break;
    }
    v->fType = eVK_Undefined;
}

Obj *live(void *p) {
    if(!g_objects.count((Obj *)p)) die("used a dead object/collection");
    return (Obj *)p;
}

PA_Variable copy_var(const PA_Variable &src) {
    PA_Variable dst = src;
    switch(src.fType) {
        case eVK_Unistring:
            dst.uValue.fString = new_string(src.uValue.fString.fString, src.uValue.fString.fLength);
            break;
        case eVK_Picture:
            dst.uValue.fPicture = new_picture();
            break;
        case eVK_Object:
            if(src.uValue.fObject) live(src.uValue.fObject)->rc++;
            break;
        case eVK_Collection:
            if(src.uValue.fCollection) live(src.uValue.fCollection)->rc++;
            break;
        default: break;
    }
    return dst;
}

Obj *new_object(bool coll = false) {
    Obj *o = new Obj();
    o->rc = 1;
    o->coll = coll;
    g_objects.insert(o);
    return o;
}

void release_object(Obj *o) {
    live(o);
    if(--o->rc == 0) {
        for(auto &kv : o->props) clear_var(&kv.second);
        for(auto &v : o->items) clear_var(&v);
        g_objects.erase(o);
        g_blob_instances.erase(o);
        delete o;
    }
}

u16 key_of(const PA_Unistring &k) { return u16(k.fString, k.fLength); }

std::string narrow(const u16 &s) { std::string r; for(PA_Unichar c : s) r.push_back((char)c); return r; }

u16 widen(const std::string &s) { u16 r; for(char c : s) r.push_back((PA_Unichar)(unsigned char)c); return r; }

PA_Variable undefined_var() { PA_Variable v; memset(&v, 0, sizeof(v)); v.fType = eVK_Undefined; return v; }

PA_Variable string_result(const u16 &s) {
    PA_Variable v = undefined_var();
    v.fType = eVK_Unistring;
    v.uValue.fString = new_string(s.c_str(), s.size());
    return v;
}

PA_Variable object_result(Obj *o) {
    PA_Variable v = undefined_var();
    v.fType = o->coll ? eVK_Collection : eVK_Object;
    if(o->coll) v.uValue.fCollection = (PA_CollectionRef)o; else v.uValue.fObject = (PA_ObjectRef)o;
    return v;
}

void execute_command(EngineBlock *eb) {
    PA_Variable **params = (PA_Variable **)eb->fPtr1;
    PA_Variable *ret = (PA_Variable *)eb->fPtr2;
    switch(eb->fParam1) {
        case 1230: { /* OB Get type */
            Obj *o = live(params[0]->uValue.fObject);
            auto it = o->props.find(key_of(params[1]->uValue.fString));
            PA_long32 kind = it == o->props.end() ? eVK_Undefined : it->second.fType;
            if(kind == eVK_Unistring) kind = eVK_Text;
            ret->fType = eVK_Longint;
            ret->uValue.fLongint = kind;
            break;
        }
        case 1231: { /* OB Is defined */
            Obj *o = live(params[0]->uValue.fObject);
            ret->fType = eVK_Boolean;
            ret->uValue.fBoolean = o->props.count(key_of(params[1]->uValue.fString)) ? 1 : 0;
            break;
        }
        case 1471: *ret = object_result(new_object(false)); break; /* New Object */
        case 1472: *ret = object_result(new_object(true)); break;  /* New Collection */
        case 1566:   /* 4D.File(platformPath; fk platform path) */
        case 1567: { /* 4D.Folder */
            if(eb->fParam2 != 2 || params[0]->fType != eVK_Unistring || params[1]->fType != eVK_Longint)
                die("unexpected 4D.File/4D.Folder parameters");
            Obj *f = new_object(false);
            g_file_objects++;
            PA_Variable path = string_result(key_of(params[0]->uValue.fString));
            f->props[widen("path")] = path;
            *ret = object_result(f);
            break;
        }
        case 1709: /* 4D */
            if(!g_4d) die("\"4D\" global not set up");
            g_4d->rc++;
            *ret = object_result(g_4d);
            break;
        case 1224: { /* OB Get */
            Obj *o = live(params[0]->uValue.fObject);
            auto it = o->props.find(key_of(params[1]->uValue.fString));
            if(it != o->props.end()) *ret = copy_var(it->second);
            break;
        }
        case 1731: { /* OB Instance of */
            Obj *o = live(params[0]->uValue.fObject);
            Obj *cls = live(params[1]->uValue.fObject);
            ret->fType = eVK_Boolean;
            ret->uValue.fBoolean = (cls == g_blob_class && g_blob_instances.count(o)) ? 1 : 0;
            break;
        }
        case 558: { /* COPY BLOB(src; dst; srcOffset; dstOffset; len) */
            Obj *src = live(params[0]->uValue.fObject);
            if(params[1]->fType != eVK_Blob) die("COPY BLOB destination is not a blob");
            Handle *h = live_handle(params[1]->uValue.fBlob.fHandle);
            size_t len = (size_t)params[4]->uValue.fLongint;
            if(len > src->bytes.size()) len = src->bytes.size();
            h->data = (char *)realloc(h->data, len ? len : 1);
            h->size = len;
            memcpy(h->data, src->bytes.data(), len);
            params[1]->uValue.fBlob.fSize = (PA_long32)len;
            break;
        }
        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "unexpected PA_ExecuteCommandByID %d", (int)eb->fParam1);
            die(msg);
        }
    }
}

void engine(short selector, EngineBlock *eb) {
    switch(selector) {
        case EX_GET_INFORMATION:
            eb->fParam2 = 0x2000; /* 4D 20: New Object path in PA_CreateObject */
            break;
        case EX_CREATE_UNISTRING: {
            const PA_Unichar *src = (const PA_Unichar *)eb->fHandle;
            size_t len = 0;
            while(src[len]) len++;
            eb->fUniString1 = new_string(src, len);
            break;
        }
        case EX_DISPOSE_UNISTRING:
            free_string(&eb->fUniString1);
            break;
        case EX_VARIABLE_TO_STRING: {
            PA_Variable *v = (PA_Variable *)eb->fHandle;
            if(v->fType == eVK_Unistring) eb->fUniString1 = v->uValue.fString; /* borrowed */
            break;
        }
        case EX_CREATE_PICTURE:
            eb->fPicture = new_picture();
            break;
        case EX_DISPOSE_PICTURE:
            free_picture(eb->fPicture);
            break;
        case EX_CLEAR_VARIABLE:
            clear_var((PA_Variable *)eb->fPtr1);
            break;
        case EX_SET_OBJ_VALUE: {
            Obj *o = live(eb->fPtr1);
            PA_Variable value = copy_var(*(PA_Variable *)eb->fPtr2);
            if(eb->fShort == eVK_Collection) {
                if(!o->coll || eb->fLongint < 0) die("bad collection set");
                size_t i = (size_t)eb->fLongint;
                if(i >= o->items.size()) o->items.resize(i + 1, undefined_var());
                clear_var(&o->items[i]);
                o->items[i] = value;
            } else {
                u16 k = key_of(eb->fUniString1);
                auto it = o->props.find(k);
                if(it != o->props.end()) clear_var(&it->second);
                o->props[k] = value;
            }
            break;
        }
        case EX_GET_OBJ_VALUE: {
            Obj *o = live(eb->fPtr1);
            PA_Variable *out = (PA_Variable *)eb->fPtr2;
            if(eb->fShort == eVK_Collection) {
                if(!o->coll) die("collection get on an object");
                *out = undefined_var();
                if(eb->fLongint == -1) {
                    out->fType = eVK_Longint;
                    out->uValue.fLongint = (PA_long32)o->items.size();
                } else if(eb->fLongint >= 0 && (size_t)eb->fLongint < o->items.size()) {
                    *out = copy_var(o->items[(size_t)eb->fLongint]);
                }
                break;
            }
            u16 k = key_of(eb->fUniString1);
            if(!g_fail_key.empty() && narrow(k) == g_fail_key) {
                g_failed_gets++;
                *out = undefined_var();
                out->fFiller = kPoisonMark;
                if(g_poison_real) {
                    out->fType = eVK_Real;
                    out->uValue.fReal = 3.0;
                } else {
                    out->fType = eVK_Unistring;
                    out->uValue.fString.fString = kPoison;
                    out->uValue.fString.fLength = 4;
                }
                eb->fError = -1;
                break;
            }
            auto it = o->props.find(k);
            *out = it == o->props.end() ? undefined_var() : copy_var(it->second);
            break;
        }
        case EX_EXECUTE_COMMAND_BY_ID:
            execute_command(eb);
            break;
        case EX_RUN_IN_MAIN_PROCESS:
            ((PA_RunInMainProcessProcPtr)eb->fPtr1)(eb->fPtr2);
            break;
        case EX_CONVERT_STRING: {
            if(eb->fParam1 != 4) die("unexpected EX_CONVERT_STRING op");
            Handle *h = live_handle(eb->fHandle);
            memcpy((void *)eb->fParam3, h->data, h->size);
            eb->fParam4 = (sLONG_PTR)h->size;
            break;
        }
        case EX_HANDLE_MANAGER: {
            switch(eb->fParam1) {
                case 1: eb->fHandle = (PA_Handle)new_handle((size_t)eb->fParam2); break;
                case 2: free_handle((Handle *)eb->fHandle); break;
                case 5: eb->fParam3 = (sLONG_PTR)live_handle(eb->fHandle)->data; break;
                case 6: break;
                default: die("unexpected handle manager op");
            }
            break;
        }
        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "unmodelled engine selector %d", selector);
            die(msg);
        }
    }
}

/* ---------- test helpers ---------- */

PA_Unistring U(const char *ascii) {
    u16 s = widen(ascii);
    return new_string(s.c_str(), s.size());
}

void put(Obj *o, const char *key, PA_Variable v) {
    PA_Unistring k = U(key);
    PA_SetObjectProperty((PA_ObjectRef)o, &k, v);
    PA_DisposeUnistring(&k);
}

PA_Variable real_var(double d) { PA_Variable v = undefined_var(); PA_SetRealVariable(&v, d); return v; }
PA_Variable bool_var(bool b) { PA_Variable v = undefined_var(); PA_SetBooleanVariable(&v, b); return v; }

void put_text(Obj *o, const char *key, const char *value) {
    PA_Variable v = undefined_var();
    PA_Unistring u = U(value);
    PA_SetStringVariable(&v, &u);
    put(o, key, v);
    PA_ClearVariable(&v);
}

/* Stores a fresh object/collection under key; the parent holds the only reference. */
Obj *put_child(Obj *o, const char *key, bool coll) {
    Obj *c = new_object(coll);
    PA_Variable v = object_result(c);
    put(o, key, v);
    PA_ClearVariable(&v);
    return c;
}

bool check(bool ok, const char *what) {
    if(!ok) fprintf(stderr, "  check failed: %s\n", what);
    return ok;
}

bool no_leaks() {
    bool ok = true;
    if(!g_strings.empty()) { fprintf(stderr, "  LEAK: %zu engine string(s)\n", g_strings.size()); ok = false; }
    if(!g_pictures.empty()) { fprintf(stderr, "  LEAK: %zu engine picture(s)\n", g_pictures.size()); ok = false; }
    if(!g_objects.empty()) { fprintf(stderr, "  LEAK: %zu engine object(s)/collection(s)\n", g_objects.size()); ok = false; }
    if(!g_handles.empty()) { fprintf(stderr, "  LEAK: %zu engine handle(s)\n", g_handles.size()); ok = false; }
    if(g_poison_clears) { fprintf(stderr, "  %d failed result(s) cleared\n", g_poison_clears); ok = false; }
    return ok;
}

/* A synthetic file on disk and a 4D.File-like object pointing at it. */
struct TempFile {
    std::string path;
    explicit TempFile(const char *contents) {
        char tmpl[] = "/tmp/extract-ownership-XXXXXX";
        int fd = mkstemp(tmpl);
        if(fd < 0) die("mkstemp");
        if(write(fd, contents, strlen(contents)) != (ssize_t)strlen(contents)) die("write");
        close(fd);
        path = tmpl;
    }
    ~TempFile() { unlink(path.c_str()); }
};

Obj *file_object(const std::string &platformPath) {
    Obj *f = new_object(false);
    put_text(f, "platformPath", platformPath.c_str());
    return f;
}

/* Calls the real PluginMain with a longint, longint, object parameter list
   and an object result slot, like 4D does for Extract(&L;&L;&J):J. */
Obj *run_extract(PA_long32 it, PA_long32 ot, Obj *document) {
    PA_long32 p1 = it, p2 = ot;
    PA_ObjectRef p3 = (PA_ObjectRef)document;
    void *params[3] = { &p1, &p2, &p3 };
    PA_ObjectRef result = nullptr;
    PluginBlock block;
    block.fParameters = params;
    block.fResult = &result;
    block.fData = 0;
    PluginMain(1, &block);
    return (Obj *)result;
}

void run_set_option(PA_long32 option, Obj *value) {
    PA_long32 p1 = option;
    PA_ObjectRef p2 = (PA_ObjectRef)value;
    void *params[2] = { &p1, &p2 };
    PluginBlock block;
    block.fParameters = params;
    block.fResult = 0;
    block.fData = 0;
    PluginMain(2, &block);
}

std::string text_prop(Obj *o, const char *key) {
    auto it = o->props.find(widen(key));
    if(it == o->props.end() || it->second.fType != eVK_Unistring) return "";
    return narrow(key_of(it->second.uValue.fString));
}

/* Sets up the "4D" global with a "Blob" class, plus a blob instance. */
Obj *setup_blob_world(const char *bytes, double size) {
    g_4d = new_object(false);
    g_blob_class = put_child(g_4d, "Blob", false);
    Obj *b = new_object(false);
    b->bytes = bytes;
    g_blob_instances.insert(b);
    put(b, "size", real_var(size));
    return b;
}

void teardown_blob_world(Obj *b) {
    while(g_objects.count(b)) release_object(b);
    release_object(g_4d);
    g_4d = nullptr;
    g_blob_class = nullptr;
}

void reset_chunker() {
    gchunker.reset();
    gpadtoken_id = 0;
}

/* ---------- tests ---------- */

typedef std::function<bool()> Test;

std::map<std::string, Test> tests() {
    std::map<std::string, Test> t;

    /* Defect A (live, support/4DPlugin-JSON.cpp): PA_GetObjectProperty
       results are owned and were never cleared. */
    t["defect_ob_get_a_result_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "k", "hello");
        CUTF8String out;
        bool ok = check(ob_get_a((PA_ObjectRef)o, L"k", &out), "defined");
        ok = check(out == CUTF8String((const uint8_t *)"hello"), "value") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_get_n_wrong_kind_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "n", "not a number");
        bool ok = check(ob_get_n((PA_ObjectRef)o, L"n") == 0.0, "wrong kind reads as 0");
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_get_b_wrong_kind_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "flag", "true");
        bool ok = check(ob_get_b((PA_ObjectRef)o, L"flag") == false, "wrong kind reads as false");
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_get_o_reference_not_released"] = [] {
        Obj *o = new_object();
        Obj *child = put_child(o, "file", false);
        PA_ObjectRef got = ob_get_o((PA_ObjectRef)o, L"file");
        bool ok = check(got == (PA_ObjectRef)child, "same object");
        ok = check(child->rc == 1, "result is borrowed from the parent") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    /* ob_get_c has no caller in this plugin (dormant helper, same fix). */
    t["defect_ob_get_c_reference_not_released"] = [] {
        Obj *o = new_object();
        Obj *c = put_child(o, "items", true);
        PA_CollectionRef got = ob_get_c((PA_ObjectRef)o, L"items");
        bool ok = check(got == (PA_CollectionRef)c, "same collection");
        ok = check(c->rc == 1, "result is borrowed from the parent") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    /* Guard: on a failed PA_GetObjectProperty the uninitialised result is
       neither read nor cleared. */
    t["guard_ob_get_error_result_untouched"] = [] {
        Obj *o = new_object();
        put_text(o, "k", "v");
        put_child(o, "c", true);
        put_child(o, "o", false);
        CUTF8String out;
        g_fail_key = "k";
        ob_get_a((PA_ObjectRef)o, L"k", &out);
        ob_get_n((PA_ObjectRef)o, L"k");
        ob_get_b((PA_ObjectRef)o, L"k");
        g_fail_key = "c";
        PA_CollectionRef c = ob_get_c((PA_ObjectRef)o, L"c");
        g_fail_key = "o";
        PA_ObjectRef so = ob_get_o((PA_ObjectRef)o, L"o");
        g_fail_key.clear();
        bool ok = check(g_failed_gets == 5, "every getter reached the engine");
        ok = check(out.empty(), "failed text result was not read") && ok;
        ok = check(c == NULL && so == NULL, "failed reference results were not read") && ok;
        release_object(o);
        return no_leaks() && ok;
    };

    /* Defect A through the shipped command: password/charset (ob_get_a),
       file (ob_get_o) and a mistyped numeric option (ob_get_n). */
    t["defect_cmd_extract_options_and_file"] = [] {
        reset_chunker();
        TempFile tf("synthetic text");
        Obj *doc = new_object();
        put_text(doc, "password", "pw");
        put_text(doc, "charset", "utf-8");
        put_text(doc, "codepage", "not a number");
        put(doc, "text_as_tokens", bool_var(false));
        Obj *file = put_child(doc, "file", false);
        put_text(file, "platformPath", tf.path.c_str());
        Obj *r = run_extract(input_type_pdf, 0, doc);
        bool ok = check(r != nullptr, "result object");
        ok = check(g_stub.parser == "pdfium" && g_stub.password == "pw", "password reached the parser") && ok;
        ok = check(g_stub.bytes == "synthetic text", "file contents reached the parser") && ok;
        ok = check(file->rc == 1, "file object borrowed from the document") && ok;
        Obj *r2 = run_extract(input_type_xls, 0, doc);
        ok = check(g_stub.charset == "utf-8", "charset reached the parser") && ok;
        if(r) release_object(r);
        if(r2) release_object(r2);
        release_object(doc);
        return no_leaks() && ok;
    };

    /* Defect B (live, _object_to_path): the 4D.File/4D.Folder object that
       PA_ExecuteCommandByID returns was never cleared. Reached from Extract
       ("file") and Extract SET OPTION (tokenizer_file). */
    t["defect_object_to_path_folder_not_cleared"] = [] {
        Obj *f = file_object("/tmp/some synthetic path");
        std::string path;
        bool ok = check(file_object_to_path((PA_ObjectRef)f, path), "converted");
        ok = check(path == "/tmp/some synthetic path", "posix path from the 4D.File") && ok;
        ok = check(g_file_objects == 1, "4D.File constructed once") && ok;
        release_object(f);
        return no_leaks() && ok;
    };
    t["defect_cmd_set_option_tokenizer_file"] = [] {
        reset_chunker();
        TempFile tf("not a real model");
        Obj *f = file_object(tf.path);
        run_set_option(option_type_tokenizer_file, f);
        bool ok = check(gchunker != nullptr && g_stub.model_path == tf.path, "chunker loaded from the path");
        reset_chunker();
        release_object(f);
        return no_leaks() && ok;
    };
    t["guard_object_to_path_platform_path_error"] = [] {
        Obj *f = file_object("/tmp/x");
        std::string path;
        g_fail_key = "platformPath";
        bool converted = folder_object_to_path((PA_ObjectRef)f, path);
        g_fail_key.clear();
        bool ok = check(g_failed_gets == 1, "getter reached the engine");
        ok = check(!converted && path.empty(), "failed platformPath was not used") && ok;
        release_object(f);
        return no_leaks() && ok;
    };
    t["guard_object_to_path_path_error"] = [] {
        Obj *f = file_object("/tmp/x");
        std::string path;
        g_fail_key = "path";
        file_object_to_path((PA_ObjectRef)f, path);
        g_fail_key.clear();
        bool ok = check(g_failed_gets == 1, "getter reached the engine");
        ok = check(path.empty(), "failed path was not used") && ok;
        release_object(f);
        return no_leaks() && ok;
    };

    /* Defect C (DORMANT: file_object_to_data has no caller). */
    t["defect_dormant_file_object_to_data_empty_owner"] = [] {
        Obj *b = setup_blob_world("synthetic blob", 14);
        b->rc++; /* isolate from the borrowed-reference check below */
        std::vector<uint8_t> data;
        bool ok = check(file_object_to_data((PA_ObjectRef)b, data), "is a blob");
        ok = check(std::string(data.begin(), data.end()) == "synthetic blob", "blob contents") && ok;
        teardown_blob_world(b);
        return no_leaks() && ok;
    };
    t["defect_dormant_file_object_to_data_releases_borrowed_input"] = [] {
        Obj *b = setup_blob_world("abc", 3);
        b->rc++; /* the test's own extra reference */
        std::vector<uint8_t> data;
        file_object_to_data((PA_ObjectRef)b, data);
        bool ok = check(b->rc == 2, "caller's reference to the blob untouched");
        teardown_blob_world(b);
        Obj *n = setup_blob_world("abc", -1); /* invalid size: early return */
        n->rc++;
        file_object_to_data((PA_ObjectRef)n, data);
        ok = check(n->rc == 2, "caller's reference untouched on invalid size") && ok;
        teardown_blob_world(n);
        return no_leaks() && ok;
    };
    t["guard_dormant_file_object_to_data_size_error"] = [] {
        Obj *b = setup_blob_world("abc", 3);
        b->rc++;
        std::vector<uint8_t> data;
        g_fail_key = "size";
        g_poison_real = true; /* a garbage size that looks valid */
        file_object_to_data((PA_ObjectRef)b, data);
        g_poison_real = false;
        g_fail_key.clear();
        bool ok = check(g_failed_gets == 1, "getter reached the engine");
        ok = check(data.empty(), "failed size was not used") && ok;
        teardown_blob_world(b);
        return no_leaks() && ok;
    };

    /* Controls: must pass on both baseline and fixed sources. */
    t["control_ob_set_and_append"] = [] {
        Obj *o = new_object();
        Obj *child = new_object();
        Obj *coll = new_object(true);
        ob_set_s((PA_ObjectRef)o, L"s", "text");
        ob_set_n((PA_ObjectRef)o, "n", 1.5);
        ob_set_b((PA_ObjectRef)o, L"b", true);
        ob_set_o((PA_ObjectRef)o, L"child", (PA_ObjectRef)child);
        ob_set_c((PA_ObjectRef)o, L"coll", (PA_CollectionRef)coll);
        Obj *items = new_object(true);
        Obj *elem = new_object();
        Obj *inner = new_object(true);
        ob_append_s((PA_CollectionRef)items, "a");
        ob_append_n((PA_CollectionRef)items, 2);
        ob_append_o((PA_CollectionRef)items, (PA_ObjectRef)elem);
        ob_append_c((PA_CollectionRef)items, (PA_CollectionRef)inner);
        bool ok = check(items->items.size() == 4, "four elements");
        ok = check(child->rc == 1 && coll->rc == 1 && elem->rc == 1 && inner->rc == 1,
                   "containers hold the only references") && ok;
        ok = check(text_prop(o, "s") == "text", "text property") && ok;
        release_object(items);
        release_object(o);
        return no_leaks() && ok;
    };
    t["control_process_paragraphs"] = [] {
        reset_chunker();
        gchunker = chunker::make_chunker(chunker::ModelConfig());
        std::vector<std::string> p = { "hello world", "", "second paragraph" };
        Obj *text = (Obj *)process_paragraphs(p, 8, true, true, 0.1f, (int)PoolingMode::Mean);
        Obj *toks = (Obj *)process_paragraphs(p, 8, true, false, 0.1f, (int)PoolingMode::LastToken);
        Obj *one = (Obj *)process_paragraph("abc", 4, true, false, 0.0f, (int)PoolingMode::LastToken);
        bool ok = check(!text->items.empty() && !toks->items.empty() && one->items.size() == 1, "chunks produced");
        release_object(text);
        release_object(toks);
        release_object(one);
        reset_chunker();
        Obj *empty = (Obj *)process_paragraph("abc", 4, true, true, 0.0f, (int)PoolingMode::Mean);
        ok = check(empty->items.size() == 1, "no chunker yields one empty chunk") && ok;
        release_object(empty);
        return no_leaks() && ok;
    };
    t["control_cmd_extract_without_file"] = [] {
        Obj *doc = new_object();
        put(doc, "tokens_length", real_var(16));
        Obj *r = run_extract(input_type_txt, 0, doc);
        bool ok = check(r && r->props.count(widen("success")), "success:false returned");
        if(r) release_object(r);
        Obj *r2 = run_extract(input_type_txt, 0, nullptr);
        ok = check(r2 != nullptr, "null document still returns an object") && ok;
        if(r2) release_object(r2);
        release_object(doc);
        return no_leaks() && ok;
    };
    t["control_init_deinit"] = [] {
        PluginBlock block; block.fParameters = 0; block.fResult = 0; block.fData = 0;
        PluginMain(kInitPlugin, &block);
        bool ok = check(g_stub.lib_inits == 2, "libraries initialised");
        PluginMain(kDeinitPlugin, &block);
        ok = check(g_stub.lib_inits == 0, "libraries shut down") && ok;
        return no_leaks() && ok;
    };
    return t;
}

} // namespace

int main(int argc, char **argv) {
    gCall4D = engine;
    std::map<std::string, Test> all = tests();
    if(argc == 2 && !strcmp(argv[1], "--list")) {
        for(auto &kv : all) printf("%s\n", kv.first.c_str());
        return 0;
    }
    if(argc != 2 || !all.count(argv[1])) {
        fprintf(stderr, "usage: %s --list | <test>\n", argv[0]);
        return 2;
    }
    return all[argv[1]]() ? 0 : 1;
}
