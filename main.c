#define  _GNU_SOURCE
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <clang-c/CXString.h>
#include <clang-c/Index.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NOB_IMPLEMENTATION
//       libclang sucks
//      /
// (._.)
#define STB_C_LEXER_IMPLEMENTATION
#include "./stb_c_lexer.h"
#include "nob.h"

typedef struct {
    char* query;
    int fuzziness;
} Options;

typedef struct {
    char* signature;
    char* file_path;
    char* name;
    bool is_defenition;
    uint32_t line;
    uint32_t col;
} Function;

typedef struct {
    Function* items;
    size_t count;
    size_t capacity;
} FunctionsDA;

typedef struct {
    Nob_String_Builder* items;
    size_t count;
    size_t capacity;
} FilesDA;


/*
     here is a proof of that
    /
{._.}

char* NormalizeSignature(char* funtion) {
    CXIndex idx = clang_createIndex(0, 0);
    struct CXUnsavedFile file_unsaved;
    file_unsaved.Contents = funtion;
    file_unsaved.Length = strlen(funtion);
    file_unsaved.Filename = "who_cares.h";
    CXTranslationUnit tu = clang_parseTranslationUnit(
            idx,
            file_unsaved.Filename, NULL, 0, &file_unsaved, 1, 
            CXTranslationUnit_None);
    if (!tu) {
        printf("Failed to parse translation unit while normalizing funtionaration %s\n", funtion);
        exit(1);
    }

    CXFile file = clang_getFile(tu, file_unsaved.Filename);
    CXSourceLocation start = clang_getLocation(tu, file, 1, 1);
    CXSourceLocation end   = clang_getLocation(tu, file, UINT_MAX, UINT_MAX);
    CXSourceRange range = clang_getRange(start, end);

    CXToken* tokens = NULL;
    unsigned tokenCount = 0;
    clang_tokenize(tu, range, &tokens, &tokenCount);

    Nob_String_Builder sb = {0};
    for (int i = 0; i<tokenCount+1; i++) {
        CXString CXtoken = clang_getTokenSpelling(tu, tokens[i]);
        char* token = clang_getCString(CXtoken);
        printf("%s\n", token);
        nob_sb_append_cstr(&sb, token);
        nob_sb_append(&sb, ' ');
        clang_disposeString(CXtoken);
    }

    clang_disposeTokens(tu, tokens, tokenCount);
    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(idx);
    return sb.items;
}
*/

//      that's so much cleaner
//     /
// (._.)

char* NormalizeSignature(char* funtion) {
    Nob_String_Builder sb = {0};
    stb_lexer lexer = {0};
    char storage[512];
    stb_c_lexer_init(&lexer, funtion, funtion+strlen(funtion), storage, sizeof(storage));
    while (stb_c_lexer_get_token(&lexer)) {
        if (lexer.token < CLEX_eof) {
            nob_sb_append(&sb, lexer.token);
        } else if (lexer.token == CLEX_id){
            nob_sb_append_cstr(&sb, lexer.string);
        } else {
            printf("unknown token %ld\n", lexer.token);
            exit(1);
        }
        nob_sb_append(&sb, ' ');
    }
    sb.items[sb.count] = '\0';
    return sb.items;
}

void PrintHelp() {
    printf("Usage: coogle2 <some-huge-lib.h> [Options]\n");
    printf("Options: \n\n");

    printf("-q --query <return_type(param_type1, param_type2 ...)>:\n\t");
    printf("Search for funtions with the following signature in the file that you specified earlier.\n\t");
    printf("If no query is provided coogle2 will just list all the funtions that it found\n\n");

    printf("-f --fuzziness <fuzziness_value>:\n\t");
    printf("Configure the fuzziness of the search.\n\t");
    printf("The bigger the fuzziness value, the less strict the search will be and the more results you will get.\n\t");
    printf("Consequently, if you search the fuzziness value of 1 coogle2 will give you the funtions that match exactly with the query.\n\t");
    printf("If the fuzziness value is not specified coogle2 will list all the funtions that it found, ranked based on how closely they match with the query.\n\n");
}

Options ParseCmdOptions(int argc, char* argv[]) {
    Options opts = {0};
    for (int i = 2; i<argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            PrintHelp();
            exit(0);
            
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--query") == 0) {
            if (argv[i+1][0] == '-' || argc < i+1) {
                printf("no query provided\n");
                printf("-q --query <return_type(param_type1, param_type2 ...)>\n");
                exit(1);
            }

            opts.query = argv[i+1];
            i++;

        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fuzziness") == 0) {
            if (argv[i+1][0] == '-' || argc < i+1) {
                printf("no fuzziness_value provided\n");
                printf("-f --fuzziness <fuzziness_value>\n\t");
                exit(1);
            }
            
            opts.fuzziness = atoi(argv[i+1]);
            i++;

        } else {
            printf("unknown option: %s\n\n", argv[i]);
            PrintHelp();
            exit(1);
        }
    }
    return opts;
}

void PrintFunctionList(FunctionsDA* list) {
    if (list->items == NULL) {
        printf("no functions found\n");
        exit(0);
    }
    for (int i = 0; i<list->count-1; i++) {
            printf("%s:%u:%u %s %s -> %s\n", list->items[i].file_path, list->items[i].line, list->items[i].col, list->items[i].name, list->items[i].is_defenition ? "(defenition)" : " ", list->items[i].signature);
    }
}

static int CursorInFile(CXCursor cur, const char *target_path) {
    CXSourceLocation loc = clang_getCursorLocation(cur);
    CXFile file;
    unsigned line, column;
    clang_getSpellingLocation(loc, &file, &line, &column, NULL);
    if (!file) return 0;

    CXString fname = clang_getFileName(file);
    const char *cpath = clang_getCString(fname);
    int ok = (cpath && strcmp(cpath, target_path) == 0);
    clang_disposeString(fname);
    return ok;
}

Function GetFunction(CXCursor cur) {
    CXType func_type = clang_getCursorType(cur);
    CXType ret_type = clang_getResultType(func_type);
    CXString name = clang_getCursorSpelling(cur);

    CXString ret_str = clang_getTypeSpelling(ret_type);
    const char* ret_cstr = clang_getCString(ret_str);

    int num_args = clang_getNumArgTypes(func_type);
    const char* param_list = clang_getCString(clang_getTypeSpelling(ret_type));
    char* params = malloc(1);
    memset(params, 0, 1);
    int prevlen = 0;
    for (int i = 0; i < num_args; i++) {
        CXType arg_type = clang_getArgType(func_type, i);
        CXString arg_str = clang_getTypeSpelling(arg_type);
        const char *arg_c = clang_getCString(arg_str);
        if (i<0) {
            prevlen = strlen(params);
        }

        params = realloc(params, prevlen + strlen(arg_c) + 3);
        memset(params+prevlen, 0, prevlen+strlen(arg_c)+3);
        if (i != 0){
            strcat(params, ", ");
            strcat(params, arg_c);
        } else {
            strcpy(params, arg_c);
        }
        clang_disposeString(arg_str);
    }

    CXSourceLocation loc = clang_getCursorLocation(cur);
    CXFile file;
    uint32_t line, column;
    clang_getSpellingLocation(loc, &file, &line, &column, NULL);
    CXString file_str = clang_getFileName(file);
    
    Function function = {0};
    char* raw_funtion = malloc(strlen(ret_cstr)+strlen(params)+3); // add 2 for prethaces () and 1 for null terminator \0
    sprintf(raw_funtion, "%s(%s)", ret_cstr, params);
    function.signature = NormalizeSignature(raw_funtion);
    function.file_path = clang_getCString(file_str);
    function.name = clang_getCString(name);
    function.col = column;
    function.line = line;
    function.is_defenition = clang_isCursorDefinition(cur);

    clang_disposeString(ret_str);
    free(params);
    return function;
}


FunctionsDA funtions = {0};
enum CXChildVisitResult visitor(CXCursor cur, CXCursor parent, CXClientData client_data) {
    const char *target = (const char *)client_data;

    if (clang_getCursorKind(cur) == CXCursor_FunctionDecl && CursorInFile(cur, target)) {   
        Function funtion = GetFunction(cur);
        nob_da_append(&funtions, funtion);
    } else return CXChildVisit_Continue;
    return CXChildVisit_Recurse;
}

int levenshtein(const char *s, const char *t) {
    size_t len_s = strlen(s);
    size_t len_t = strlen(t);

    if (len_s == 0) return (int)len_t;
    if (len_t == 0) return (int)len_s;

    if (len_s < len_t) {
        const char *tmp = s; s = t; t = tmp;
        size_t tmp_len = len_s; len_s = len_t; len_t = tmp_len;
    }

    int* prev = malloc((len_t + 1) * sizeof(int));
    int* cur  = malloc((len_t + 1) * sizeof(int));
    if (!prev || !cur) { perror("yeaah boiiiiiiiiiii, malloc error"); exit(EXIT_FAILURE); }

    for (size_t j = 0; j <= len_t; ++j)
        prev[j] = (int)j;

    for (size_t i = 1; i <= len_s; ++i) {
        cur[0] = (int)i;
        for (size_t j = 1; j <= len_t; ++j) {
            int cost = (s[i-1] == t[j-1]) ? 0 : 1;
            int deletion    = prev[j] + 1;
            int insertion   = cur[j-1] + 1;
            int substitution = prev[j-1] + cost;
            int best = deletion;
            if (insertion < best)   best = insertion;
            if (substitution < best)best = substitution;
            cur[j] = best;
        }

        int *tmp = prev; prev = cur; cur = tmp;
    }

    int distance = prev[len_t];
    free(prev);
    free(cur);
    return distance;
}

int FunctionsComparator(const void* a, const void* b, void* needle) {
    Function* da = (Function*)a;
    Function* db = (Function*)b;
    return levenshtein(da->signature, (char*)needle) - levenshtein(db->signature, (char*)needle);
}

FunctionsDA SearchFunctions(FunctionsDA* haystack, Options* opts) {
    char* needle = NormalizeSignature(opts->query);
    
    if (opts->fuzziness <= 0) {
        qsort_r(funtions.items, funtions.count, sizeof(*funtions.items), FunctionsComparator, needle);
        return funtions;
    }

    FunctionsDA candidates = {0};
    for (int i = 0; i<haystack->count; i++) {
        if (levenshtein(needle, haystack->items[i].signature) < opts->fuzziness) {
            nob_da_append(&candidates, haystack->items[i]);
        }
    }
    qsort_r(candidates.items, candidates.count, sizeof(*candidates.items), FunctionsComparator, needle);
    return candidates;
}

void SearchInFile(char* file, Options* opts) {
}

bool IsDir(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        PrintHelp();
        return 1;
    }
    Options opts = ParseCmdOptions(argc, argv);

    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit tu = clang_parseTranslationUnit(
            index,
            argv[1],
            NULL, 0,
            NULL, 0,
            CXTranslationUnit_None);

    if (!tu) {
        fprintf(stderr, "Failed to parse %s\n", argv[1]);
        clang_disposeIndex(index);
        exit(1);
    }

    CXCursor root = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(root, visitor, (CXClientData)argv[1]);

    if (!opts.query) {
        printf("WARNING: no query provided\n\n");
        PrintFunctionList(&funtions);
        clang_disposeTranslationUnit(tu);
        clang_disposeIndex(index);
        exit(0);
    }

    FunctionsDA results = SearchFunctions(&funtions, &opts);
    PrintFunctionList(&results);

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);

    return 0;
}
