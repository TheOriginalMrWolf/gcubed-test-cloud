#ifndef CODEGEN_H
#define CODEGEN_H

extern char *(*codegen_begin_func )();
extern char *(*codegen_end_func   )();
extern char *(*codegen_show_symbol)();
extern void  (*codegen_begin_block)();
extern void  (*codegen_begin_eqn  )();
extern void  (*codegen_begin_file )();
extern void  (*codegen_declare    )();
extern void  (*codegen_end_eqn    )();
extern void  (*codegen_end_file   )();
extern void  (*codegen_show_eq    )();
extern char *(*codegen_show_node  )();
extern void  (*codegen_write_file )();
extern void  (*codegen_wrap_write )();
extern char *(*codegen_spprint    )();

void set_language(char *lang);

#endif /* CODEGEN_H */
