#include <list>
#include <string>
#include <cstring>
#include <ciso646>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace std;

// *****************************************************************************
#define STR(...) #__VA_ARGS__
#define STRINGIFY(...) STR(__VA_ARGS__)

// *****************************************************************************
// * Hashing, used here and embedded in the code
// *****************************************************************************
#define HASH_SRC                                                        \
   template <typename T> struct __hash {                                \
      size_t operator()(const T& h) const noexcept {                    \
         return std::hash<T>{}(h); }                                    \
   };                                                                   \
   template <class T> inline                                            \
   size_t hash_combine(const size_t &s, const T &v) noexcept {          \
      return s^(__hash<T>{}(v)+0x9e3779b9ull+(s<<6)+(s>>2));            \
   }                                                                    \
   template<typename T>                                                 \
   size_t hash_args(const size_t &s, const T &t) noexcept {             \
      return hash_combine(s,t);                                         \
   }                                                                    \
   template<typename T, typename... Args>                               \
   size_t hash_args(const size_t &s, const T &f, Args... a) noexcept {  \
      return hash_args(hash_combine(s,f), a...);                        \
   }
HASH_SRC

// *****************************************************************************
// * STRUCTS: argument, tpl_t, kernel, context, error
// *****************************************************************************
struct argument {
   string type, name;
   bool star, is_const, restrict, is_template;
   std::list<int> range;
  argument(): star(false), is_const(false), restrict(false) {}
};

// *****************************************************************************
struct tpl_t {
   string std_args;
   string std_parameters;
   string template_parameters;
   string template_args;
   list<list<int> > ranges;
   string return_type;
   string signature;
};

// *****************************************************************************
struct kernel{
   bool jit;
   string xcc;
   string dirname;
   string name;
   string static_format;
   string static_args;
   string static_tmplt;
   string any_pointer_params;
   string any_pointer_args;
   string d2u;
   string u2d;
   bool T;
   struct tpl_t tpl;
};

// *****************************************************************************
struct context {
#ifdef MFEM_USE_GPU
   const bool mm = true;
#else
   const bool mm = false;
#endif
#ifdef MFEM_USE_JIT
   const bool jit = true;
#else
   const bool jit = false;
#endif
   int line;
   int compound_statements;
   string& file;
   istream& in;
   ostream& out;
   std::list<argument> args;
   kernel ker;
public:
   context(istream& i, ostream& o, string &f)
      : line(1), compound_statements(-2), file(f), in(i), out(o){}
};

// *****************************************************************************
struct error {
   int line;
   string file;
   const char *msg;
   error(int l, string f, const char *m): line(l), file(f), msg(m) {}
};

// *****************************************************************************
const char* strrnc(const char* s, const unsigned char c, int n=1) {
   size_t len = strlen(s);
   char* p = (char*)s+len-1;
   for (; n; n--,p--,len--) {
      for (; len; p--,len--)
         if (*p==c) break;
      if (! len) return NULL;
      if (n==1) return p;
   }
   return NULL;
}

// *****************************************************************************
void check(context &pp, const bool test, const char *msg = NULL){
   if (not test) throw error(pp.line,pp.file,msg);
}

// *****************************************************************************
int help(char* argv[]) {
   cout << "MFEM preprocessor:";
   cout << argv[0] << " -o output input" << endl;
   return ~0;
}

// *****************************************************************************
static inline bool is_newline(const int ch) {
   return static_cast<unsigned char>(ch) == '\n';
}

// *****************************************************************************
static inline int get(context &pp) { return pp.in.get(); }

// *****************************************************************************
static inline int put(const char c, context &pp) {
   if (is_newline(c)) pp.line++;
   pp.out.put(c);
   return c;
}

// *****************************************************************************
static inline int put(context &pp) { return put(get(pp),pp); }

// *****************************************************************************
void skip_space(context &pp, string &out) {
   while (isspace(pp.in.peek())) out += get(pp);
}

// *****************************************************************************
void skip_space(context &pp) {
   while (isspace(pp.in.peek())) put(pp);
}

// *****************************************************************************
void drop_space(context &pp) {
   while (isspace(pp.in.peek())) get(pp);
}

// *****************************************************************************
bool is_comments(context &pp) {
   if (pp.in.peek() != '/') return false;
   pp.in.get();
   assert(!pp.in.eof());
   const int c = pp.in.peek();
   pp.in.unget();
   if (c == '/' || c == '*') return true;
   return false;
}

// *****************************************************************************
void singleLineComments(context &pp) {
   while (not is_newline(pp.in.peek())) put(pp);
}

// *****************************************************************************
void blockComments(context &pp) {
   for(char c;pp.in.get(c);) {
      put(c,pp);
      if (c == '*' && pp.in.peek() == '/') {
         put(pp);
         skip_space(pp);
         return;
      }
   }
}

// *****************************************************************************
void comments(context &pp) {
   if (not is_comments(pp)) return;
   put(pp);
   if (put(pp) == '/') return singleLineComments(pp);
   return blockComments(pp);
}

// *****************************************************************************
static inline bool is_id(context &pp) {
   const unsigned char c = pp.in.peek();
   return isalnum(c) or c == '_';
}

// *****************************************************************************
string get_id(context &pp) {
   string str;
   check(pp,is_id(pp),"Name w/o alnum 1st letter");
   while (is_id(pp)) str += pp.in.get();
   return str;
}

// *****************************************************************************
bool is_digit(context &pp) {
   const unsigned char c = pp.in.peek();
   return isdigit(c);
}

// *****************************************************************************
int get_digit(context &pp) {
   string str;
   check(pp,is_digit(pp),"Unknown number");
   while (is_digit(pp)) str += pp.in.get();
   return atoi(str.c_str());
}

// *****************************************************************************
string get_directive(context &pp) {
   string str;
   while (is_id(pp)) str += pp.in.get();
   return str;
}

// *****************************************************************************
string peekn(context &pp, const int n) {
   int k = 0;
   assert(n<64);
   static char c[64];
   for (k=0;k<=n;k+=1) c[k] = 0;
   for (k=0; k<n; k+=1) {
      c[k] = pp.in.get();
      if (pp.in.eof()) break;
   }
   string rtn = c;
   for (int l=0; l<k; l+=1) pp.in.unget();
   assert(!pp.in.fail());
   return rtn;
}

// *****************************************************************************
string peekID(context &pp) {
   int k = 0;
   const int n = 64;
   static char c[64];
   for (k=0;k<n;k+=1) c[k] = 0;
   for (k=0; k<n; k+=1) {
      if (! is_id(pp)) break;
      c[k]=pp.in.get();
      assert(not pp.in.eof());
   }
   string rtn(c);
   for (int l=0; l<k; l+=1) pp.in.unget();
   return rtn;
}

// *****************************************************************************
static inline void drop_name(context &pp) {
   while (is_id(pp)) get(pp);
}

// *****************************************************************************
static inline void drop_name(context &pp, string &out) {
   while (is_id(pp)) out += get(pp);
}

// *****************************************************************************
bool isvoid(context &pp) {
   skip_space(pp);
   const string void_peek = peekn(pp,4);
   assert(not pp.in.eof());
   if (void_peek == "void") return true;
   return false;
}

// *****************************************************************************
bool isstatic(context &pp) {
   skip_space(pp);
   const string void_peek = peekn(pp,6);
   assert(not pp.in.eof());
   if (void_peek == "static") return true;
   return false;
}

// *****************************************************************************
bool is_star(context &pp) {
   skip_space(pp);
   if (pp.in.peek() == '*') return true;
   return false;
}

// *****************************************************************************
bool is_coma(context &pp) {
   skip_space(pp);
   if (pp.in.peek() == ',') return true;
   return false;
}

// *****************************************************************************
static inline void hashHeader(context &pp){
   pp.out << "#include <cstddef>\n";
   pp.out << "#include <functional>\n";	
   pp.out << STRINGIFY(HASH_SRC) << "\n";
}

// *****************************************************************************
// * JIT
// *****************************************************************************
void jitHeader(context &pp){
   if (not pp.jit) return;
   pp.out << "#include \"../../general/okrtc.hpp\"\n";
}

// *****************************************************************************
void jitKernelArgs(context &pp){
   if (not pp.jit or not pp.ker.jit) return;
   pp.ker.xcc = STRINGIFY(MFEM_CXX) " " \
      STRINGIFY(MFEM_BUILD_FLAGS) " " \
      "-O3 -std=c++11 -Wall";
   pp.ker.dirname = STRINGIFY(MFEM_SRC);
   pp.ker.static_args.clear();
   pp.ker.static_tmplt.clear();
   pp.ker.static_format.clear();
   pp.ker.any_pointer_args.clear();
   pp.ker.any_pointer_params.clear();
   pp.ker.d2u.clear();
   pp.ker.u2d.clear();
   
   for(std::list<argument>::iterator ia = pp.args.begin();
       ia != pp.args.end() ; ia++) {
      const argument &arg = *ia;
      const bool is_const = arg.is_const;
      //const bool is_restrict = a->restrict;
      const bool is_pointer = arg.star;
      const char *type = arg.type.c_str();
      const char *name = arg.name.c_str();
      if (is_const && ! is_pointer){
         const bool is_double = strcmp(type,"double")==0;
         if (! pp.ker.static_format.empty()) pp.ker.static_format += ",";
         pp.ker.static_format += is_double?"0x%lx":"%ld";
         if (! pp.ker.static_args.empty()) pp.ker.static_args += ",";
         pp.ker.static_args += is_double?"u":"";
         pp.ker.static_args += name;
         if (! pp.ker.static_tmplt.empty()) pp.ker.static_tmplt += ",";
         pp.ker.static_tmplt += "const ";
         pp.ker.static_tmplt += is_double?"uint64_t":type;
         pp.ker.static_tmplt += " ";
         pp.ker.static_tmplt += is_double?"t":"";
         pp.ker.static_tmplt += name;
         if (is_double){
            {
               pp.ker.d2u += "const double ";
               pp.ker.d2u += name;
               pp.ker.d2u += " = (union_du){u:t";
               pp.ker.d2u += name;
               pp.ker.d2u += "}.d;";
            }
            {
               pp.ker.u2d += "const uint64_t u";
               pp.ker.u2d += name;
               pp.ker.u2d += " = (union_du){";
               pp.ker.u2d += name;
               pp.ker.u2d += "}.u;";
            }
         }
      }
      if (is_const && is_pointer){
         if (! pp.ker.any_pointer_args.empty()) pp.ker.any_pointer_args += ",";
         pp.ker.any_pointer_args += name;
         if (! pp.ker.any_pointer_params.empty()) {
            pp.ker.any_pointer_params += ",";
         }
         {
            pp.ker.any_pointer_params += "const ";
            pp.ker.any_pointer_params += type;
            pp.ker.any_pointer_params += " *";
            pp.ker.any_pointer_params += (pp.mm?"_":"");
            pp.ker.any_pointer_params += name;
         }
      }
      if (! is_const && is_pointer){
         if (! pp.ker.any_pointer_args.empty()) pp.ker.any_pointer_args += ",";
         pp.ker.any_pointer_args += name;
         if (! pp.ker.any_pointer_params.empty()){
            pp.ker.any_pointer_params += ",";
         }
         {
            pp.ker.any_pointer_params += type;
            pp.ker.any_pointer_params += " *";
            pp.ker.any_pointer_params += (pp.mm?"_":"");
            pp.ker.any_pointer_params += name;
         }
      }
   }
}

// *****************************************************************************
void jitPrefix(context &pp){
   if (not pp.jit or not pp.ker.jit) return;
   pp.out << "\n\tconst char *src=R\"_(\n";
   pp.out << "#include <cstdint>";
   pp.out << "\n#include <cstring>";
   pp.out << "\n#include <stdbool.h>";
   pp.out << "\n#include \"general/okina.hpp\"";
   pp.out << "\ntypedef union {double d; uint64_t u;} union_du;";
   pp.out << "\ntemplate<" << pp.ker.static_tmplt << ">";
   pp.out << "\nvoid jit_" << pp.ker.name << "(";
   pp.out << pp.ker.any_pointer_params << "){";
   if (not pp.ker.d2u.empty()) pp.out << "\n\t" << pp.ker.d2u;
   // Starts counting the compound statements
   pp.compound_statements = 0;
}

// *****************************************************************************
void jitPostfix(context &pp){
   if (not pp.jit or not pp.ker.jit) return;
   if (pp.compound_statements>=0 && pp.in.peek() == '{') { pp.compound_statements++; }
   if (pp.compound_statements>=0 && pp.in.peek() == '}') { pp.compound_statements--; }
   if (pp.compound_statements!=-1) return;
   pp.out << "}";
   pp.out << "\nextern \"C\" void k%016lx("
          << pp.ker.any_pointer_params << "){";
	pp.out << "jit_"<<pp.ker.name
          << "<" << pp.ker.static_format<<">"
          << "(" << pp.ker.any_pointer_args << ");";
   pp.out << "})_\";";
   // typedef, hash map and launch
   pp.out << "\n\ttypedef void (*kernel_t)("<<pp.ker.any_pointer_params<<");";
   pp.out << "\n\tstatic std::unordered_map<size_t,ok::okrtc<kernel_t>*> __kernels;";
   if (not pp.ker.u2d.empty()) pp.out << "\n\t" << pp.ker.u2d;

   pp.out << "\n\tconst char *xcc = \"" << pp.ker.xcc << "\";";
   pp.out << "\n\tconst size_t args_seed = std::hash<size_t>()(0);";
   pp.out << "\n\tconst size_t args_hash = ok::hash_args(args_seed,"
          << pp.ker.static_args << ");";
   pp.out << "\n\tif (!__kernels[args_hash]){";
   pp.out << "\n\t\t__kernels[args_hash] = new ok::okrtc<kernel_t>"
          << "(xcc,src," << "\"-I" << pp.ker.dirname << "\","
          << pp.ker.static_args << ");";
   pp.out << "}\n\t(__kernels[args_hash]->operator_void("
          << pp.ker.any_pointer_args << "));\n";
   // Stop counting the compound statements and flush the JIT status
   pp.compound_statements--;
   pp.ker.jit = false;
}

// *****************************************************************************
bool get_args(context &pp) {
   bool empty = true;
   argument arg;
   pp.args.clear();
   
   // Go to first possible argument
   skip_space(pp);
   if (isvoid(pp)) { // if it is 'void' drop it
      drop_name(pp);
      return true;
   }
   for (int p=0; true; empty=false) {
      if (is_star(pp)){
         arg.star = true;
         put(pp);
         continue;
      }
      if (is_coma(pp)){
         put(pp);
         continue;
      }
      const string &id = peekID(pp);
      drop_name(pp);
      // Qualifiers
      if (id=="const") { pp.out << id; arg.is_const = true; continue; }
      if (id=="__restrict") { pp.out << id; arg.restrict = true; continue; }
      // Types
      if (id=="char") { pp.out << id; arg.type = id; continue; }
      if (id=="int") { pp.out << id; arg.type = id; continue; }
      if (id=="short") { pp.out << id; arg.type = id; continue; }
      if (id=="unsigned") { pp.out << id; arg.type = id; continue; }
      if (id=="long") { pp.out << id; arg.type = id; continue; }
      if (id=="bool") { pp.out << id; arg.type = id; continue; }
      if (id=="float") { pp.out << id; arg.type = id; continue; }
      if (id=="double") { pp.out << id; arg.type = id; continue; }
      if (id=="size_t") { pp.out << id; arg.type = id; continue; }
      const bool jit = pp.ker.jit;
      pp.out << ((jit or not pp.mm)?"":"_") << id;
      // focus on the name, we should have qual & type
      arg.name = id;
      pp.args.push_back(arg);
      arg = argument();
      const int c = pp.in.peek();
      assert(not pp.in.eof());
      if (c == '(') p+=1;
      if (c == ')') p-=1;
      if (p<0) { break; }
      skip_space(pp);
      check(pp,pp.in.peek()==',',"No coma while in args");
      put(pp);
   }
   // Prepare the JIT strings from the arguments
   jitKernelArgs(pp);
   return empty;
}

// *****************************************************************************
void genPtrOkina(context &pp){
   // Generate the GET_* code
   for(std::list<argument>::iterator ia = pp.args.begin();
       ia != pp.args.end() ; ia++) {
      const argument a = *ia;
      const bool is_const = a.is_const;
      //const bool is_restrict = a.restrict;
      const bool is_pointer = a.star;
      const char *type = a.type.c_str();
      const char *name = a.name.c_str();
      if (is_const && ! is_pointer){
         if (!pp.ker.jit){
            pp.out << "\n\tconst " << type << " " << name
                   << " = (const " << type << ")"
                   << " (_" << name << ");";
         }
      }
      if (is_const && is_pointer){
         pp.out << "\n\tconst " << type << "* " << name
                << " = (const " << type << "*)"
                << " mfem::mm::adrs(_" << name << ");";
      }
      if (! is_const && is_pointer){
         pp.out << "\n\t" << type << "* " << name
                << " = (" << type << "*)"
                << " mfem::mm::adrs(_" << name << ");";
      }
   }
}

// *****************************************************************************
void __kernel(context &pp) {
   // Skip   "__kernel"
   pp.out << "        ";
   skip_space(pp);
   check(pp,isvoid(pp) or isstatic(pp),"Kernel w/o void or static");
   if (isstatic(pp)) {
      pp.out << get_id(pp);
      skip_space(pp);
   }
   const string void_return_type = get_id(pp);
   pp.out << void_return_type;
   // Get kernel's name
   skip_space(pp);
   const string name = get_id(pp);
   pp.out << name;
   pp.ker.name = name;
   skip_space(pp);
   // check we are at the left parenthesis
   check(pp,pp.in.peek()=='(',"No 1st '(' in kernel");
   put(pp);
   // Get the arguments   
   get_args(pp);
   // Make sure we have hit the last ')' of the arguments
   check(pp,pp.in.peek()==')',"No last ')' in kernel");
   put(pp);
   skip_space(pp);
   // Make sure we are about to start a compound statement
   check(pp,pp.in.peek()=='{',"No compound statement found");
   put(pp);
   // Generate the JIT prefix for this kernel
   jitPrefix(pp);
   // If we are using the memory manager, generate the calls
   if (pp.mm) genPtrOkina(pp);
}

// *****************************************************************************
void __jit(context &pp){
   // Skip "__jit"
   pp.out << "   ";
   skip_space(pp);
   comments(pp);
   pp.ker.jit = true;
   string id = get_id(pp);
   check(pp,id=="__kernel","No 'kernel' keyword after 'jit' qualifier");
   __kernel(pp);
}

// *****************************************************************************
// * Template generation
// *****************************************************************************
void __range(context &pp, argument &arg){
   char c;
   bool dash = false;
   // Verify and eat '('
   check(pp,get(pp)=='(',"__template should declare its range");
   do{
      const int n = get_digit(pp);
      if (dash){
         for(int i=arg.range.back()+1;i<n;i++){
            arg.range.push_back(i);
         }
      }
      dash = false;
      arg.range.push_back(n);
      c = get(pp);
      assert(not pp.in.eof());
      check(pp, c==',' or c=='-' or  c==')', "Unknown __template range");
      if (c=='-'){
         dash = true;
      }
   } while(c!=')');
}

// *****************************************************************************
void get_targs(context &pp) {
   int nargs = 0;
   int targs = 0;
   argument arg;
   pp.args.clear();
   // Go to first possible argument
   drop_space(pp);
   if (isvoid(pp)) { assert(false); }
   string current_arg;
   for (int p=0; true;) {
      skip_space(pp,current_arg);
      comments(pp);
      if (is_star(pp)){
         arg.star = true;
         current_arg += get(pp);
         continue;
      }
      skip_space(pp,current_arg);
      comments(pp);
      if (is_coma(pp)){
         current_arg += get(pp);
         continue;
      }
      const string &id = peekID(pp);
      drop_name(pp);
      // Qualifiers
      if (id=="__range") { __range(pp,arg); arg.is_template = true; continue; }
      if (id=="const") { current_arg += id; arg.is_const = true; continue; }
      if (id=="__restrict") { current_arg += id; arg.restrict = true; continue; }
      // Types
      if (id=="char") { current_arg += id; arg.type = id; continue; }
      if (id=="int") { current_arg += id; arg.type = id; continue; }
      if (id=="short") { current_arg += id; arg.type = id; continue; }
      if (id=="unsigned") { current_arg += id; arg.type = id; continue; }
      if (id=="long") { current_arg += id; arg.type = id; continue; }
      if (id=="bool") { current_arg += id; arg.type = id; continue; }
      if (id=="float") { current_arg += id; arg.type = id; continue; }
      if (id=="double") { current_arg += id; arg.type = id; continue; }
      if (id=="size_t") { current_arg += id; arg.type = id; continue; }
      // focus on the name, we should have qual & type
      arg.name = id;
      if (not arg.is_template){
         pp.args.push_back(arg);
         pp.ker.tpl.signature += current_arg + (not pp.mm?"":"_") + id;
         {
            pp.ker.tpl.std_args += (nargs==0)?"":", ";
            pp.ker.tpl.std_args +=  arg.name;
         }
         nargs += 1;
      }else{
         pp.ker.tpl.template_parameters += (targs==0)?"":", ";
         pp.ker.tpl.template_parameters += "const " + arg.type + " " + arg.name;
         pp.ker.tpl.ranges.push_back(arg.range);
         //pp.ker.tpl.template_parameters += "/*";
         //for (int n : arg.range) { pp.ker.tpl.template_parameters += std::to_string(n) + " "; }
         //pp.ker.tpl.template_parameters += "*/";
         {
            pp.ker.tpl.template_args += (targs==0)?"":", ";
            pp.ker.tpl.template_args += arg.name;
         }
         targs += 1;
      }
      pp.ker.tpl.std_parameters += current_arg + id + (nargs==0 and targs>0?",":"");
      arg = argument();
      current_arg = string();
      const int c = pp.in.peek();
      assert(not pp.in.eof());
      if (c == '(') p+=1;
      if (c == ')') p-=1;
      if (p<0) { break; }
      skip_space(pp,current_arg);
      comments(pp);
      check(pp,pp.in.peek()==',',"<__template> No coma while in args");
      get(pp);
      if (nargs>0) current_arg += ",";
   }
}

// *****************************************************************************
void __template(context &pp){
   pp.ker.T = true;
   pp.ker.tpl = tpl_t();
   drop_space(pp);
   comments(pp);
   string id = get_id(pp);
   check(pp,id=="__kernel","No 'kernel' keyword after 'template' qualifier");
   check(pp,isvoid(pp) or isstatic(pp),"Templated kernel w/o void or static");
   if (isstatic(pp)) {
      pp.ker.tpl.return_type += get_id(pp);
      skip_space(pp,pp.ker.tpl.return_type);
   }
   const string void_return_type = get_id(pp);
   pp.ker.tpl.return_type += void_return_type;
   // Get kernel's name
   skip_space(pp,pp.ker.tpl.return_type);
   const string name = get_id(pp);
   pp.ker.name = name;
   skip_space(pp, pp.ker.tpl.return_type);
   // check we are at the left parenthesis
   check(pp,pp.in.peek()=='(',"No 1st '(' in kernel");
   get(pp);
   // Get the arguments   
   get_targs(pp);
   // Make sure we have hit the last ')' of the arguments
   check(pp,pp.in.peek()==')',"No last ')' in kernel");
   pp.ker.tpl.signature += get(pp);
   // Now dump the templated kernel needs before the body
   pp.out << "template<";
   pp.out << pp.ker.tpl.template_parameters;
   pp.out << ">\n";
   pp.out << pp.ker.tpl.return_type;
   pp.out << "__" << pp.ker.name;
   pp.out << "(" << pp.ker.tpl.signature;
   // Std body dump to pp.out
   skip_space(pp);
   // Make sure we are about to start a compound statement
   check(pp,pp.in.peek()=='{',"<>No compound statement found");
   put(pp);
   // If we are using the memory manager, generate the calls
   if (pp.mm) { genPtrOkina(pp); }
   // Starts counting the compound statements
   pp.compound_statements = 0;
}

// *****************************************************************************
list<list<int> > outer_product(const list<list<int> > &v) {
    list<list<int> > s = {{}};
    for (const auto &u : v) {
        list<list<int> > r;
        for (const auto &x:s) {
            for (const auto y:u) {
                r.push_back(x);
                r.back().push_back(y);
            }
        }
        s = std::move(r);
    }
    return s;
}

// *****************************************************************************
void tplPostfix(context &pp){
   if (not pp.ker.T) return;
   if (pp.compound_statements>=0 && pp.in.peek() == '{') { pp.compound_statements++; }
   if (pp.compound_statements>=0 && pp.in.peek() == '}') { pp.compound_statements--; }
   if (pp.compound_statements!=-1) return;
   check(pp,pp.in.peek()=='}',"<>No compound statements found");
   put(pp);
   // Stop counting the compound statements and flush the T status
   pp.compound_statements--;
   pp.ker.T = false;
   // Now push template kernel launcher
   pp.out << "\n// *****************************************************************************\n";
   pp.out << pp.ker.tpl.return_type;
   pp.out << pp.ker.name;
   pp.out << "(" << pp.ker.tpl.std_parameters;
   pp.out << "){";
   pp.out << "\n\ttypedef ";
   pp.out << pp.ker.tpl.return_type << "(*__T" << pp.ker.name << ")";
   pp.out << "(" << pp.ker.tpl.signature << ";";
   pp.out << "\n\tconst size_t id = hash_args(std::hash<size_t>()(0), "
          << pp.ker.tpl.template_args
          << ");";
   pp.out << "\n\tstatic std::unordered_map<size_t, "
          << "__T" << pp.ker.name << "> call = {";
   for (list<int> range : outer_product(pp.ker.tpl.ranges)) {
      pp.out << "\n\t\t{";
      int i=1;
      const int n = range.size();
      size_t hash = 0;
      for (int r : range) hash = hash_args(hash,r);
      pp.out << std::hex << "0x" << hash ;// <<"ul";
      pp.out << ",&__"<<pp.ker.name<<"<";
      for (int r : range) {
         pp.out << to_string(r) << (i==n?"":",");
         i+=1;
      }
      pp.out << ">},";
   }
   pp.out << "\n\t};";
   pp.out << "\n\tassert(call[id]);";
   pp.out << "\n\tcall[id](";
   pp.out << pp.ker.tpl.std_args;
   pp.out << ");";
   pp.out << "\n}";
}

// *****************************************************************************
void tokens(context &pp) {
   if (pp.in.peek() != '_') return;
   string id = get_id(pp);
   if (id=="__jit"){ return __jit(pp); }
   if (id=="__kernel"){ return __kernel(pp); }
   if (id=="__template"){ return __template(pp); }
   pp.out << id;
}

// *****************************************************************************
bool eof(context &pp){
   const int c = pp.in.get();
   if (pp.in.eof()) return true;
   put(c,pp);
   return false;
}

// *****************************************************************************
int process(context &pp) {
   jitHeader(pp);
   hashHeader(pp);
   pp.ker.T = false;
   pp.ker.jit = false;
   do{
      tokens(pp);
      comments(pp);
      jitPostfix(pp);
      tplPostfix(pp);
   } while (not eof(pp));
   return 0;
}

// *****************************************************************************
int main(const int argc, char* argv[]) {
   string input, output, file;   
   if (argc<=1) return help(argv);
   for (int i=1; i<argc; i+=1) {
      // -h lauches help
      if (argv[i] == string("-h"))
         return help(argv);
      // -o fills output
      if (argv[i] == string("-o")) {
         output = argv[i+1];
         i+=1;
         continue;
      }
      // should give input file
      const char* last_dot = strrnc(argv[i],'.');
      const size_t ext_size = last_dot?strlen(last_dot):0;
      if (last_dot && ext_size>0) {
         assert(file.size()==0);
         file = input = argv[i];
      }
   }
   assert(! input.empty());
   const bool output_file = ! output.empty();
   ifstream in(input.c_str(), ios::in | std::ios::binary);
   ofstream out(output.c_str(), ios::out | std::ios::binary | ios::trunc);
   assert(!in.fail());
   assert(in.is_open());
   if (output_file) {assert(out.is_open());}
   context pp(in,output_file?out:cout,file);
   try {
      process(pp);
   } catch (error err) {
      cerr << endl
	   << err.file << ":" << err.line << ":"
           << " mpp error"
           << (err.msg?": ":"")
           << (err.msg?err.msg:"")
           << endl;
      remove(output.c_str());
      return ~0;
   }
   in.close();
   out.close();
   return 0;
}
