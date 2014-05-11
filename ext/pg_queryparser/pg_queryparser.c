#include "postgres.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include "utils/memutils.h"
#include "parser/parser.h"
#include "nodes/print.h"

#include <ruby.h>

const char* progname = "pg_queryparser";

static VALUE pg_queryparser_raw_parse(VALUE self, VALUE input)
{
	Check_Type(input, T_STRING);
	
	MemoryContext ctx = NULL;
	List *tree;
	char *str;
	VALUE result;
	ErrorData* error = NULL;

	ctx = AllocSetContextCreate(TopMemoryContext,
								"RootContext",
								ALLOCSET_DEFAULT_MINSIZE,
								ALLOCSET_DEFAULT_INITSIZE,
								ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(ctx);

	str = StringValueCStr(input);
	
	PG_TRY();
	{
		tree = raw_parser(str);
	}
	PG_CATCH();
	{
		error = CopyErrorData();
		FlushErrorState();
	}
	PG_END_TRY();
	
	if (tree && error == NULL) {
		str = nodeToString(tree);
		result = rb_tainted_str_new_cstr(str);
		pfree(str);
	}
	
	MemoryContextSwitchTo(TopMemoryContext);
	MemoryContextDelete(ctx);
	
	if (tree == NULL || error != NULL) {
		VALUE cPgQueryparser, cParseError;
		VALUE exc, args[2];
		
		cPgQueryparser = rb_const_get(rb_cObject, rb_intern("PgQueryparser"));
		cParseError = rb_const_get_at(cPgQueryparser, rb_intern("ParseError"));
		
		if (error) {
			args[0] = rb_tainted_str_new_cstr(error->message);
			args[1] = INT2NUM(error->cursorpos);
		} else {
			args[0] = rb_str_new2("unknown parser error");
			args[1] = Qnil;
		}
		
		exc = rb_class_new_instance(2, args, cParseError);
		
		rb_exc_raise(exc);
	}
	
	return result;
}

void Init_pg_queryparser(void)
{
	VALUE cPgQueryparser;
	
	MemoryContextInit();

	cPgQueryparser = rb_const_get(rb_cObject, rb_intern("PgQueryparser"));

	rb_define_singleton_method(cPgQueryparser, "_raw_parse", pg_queryparser_raw_parse, 1);
}