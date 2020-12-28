//strpcom-v5.c:
//$ make strpcom-v5 CFLAGS='-std=c99 -Wall -pedantic -Wextra -Wno-switch'

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 0
#endif
static const int debug_level = DEBUG_LEVEL;
static const int handle_roots = 1;


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef union uobject *object;
typedef object list;
typedef enum tag { INVALID, INTEGER, LIST } tag;
#define OBJECT(...) new_( (union uobject[]){{ __VA_ARGS__ }} )

union uobject { tag t;
       struct { tag t; int i, continues; } Int;
       struct { tag t; object a, b; } List;
};

typedef struct record {
  int mark;
  struct record *prev;
  object *handle;
  union uobject o;
} record;


static void     init_patterns();
static void     cleanup();
static list     chars_from_file( FILE *f );
static list     logical_lines( list o );
static list       trim_continue( list o, list tail );
static list     restore_continues( list o );
static list     strip_comments( list o );
static list       single_remainder( list tail );
static list       multi_remainder( list tail );
static int        starts_literal( object a );
static list       literal_val( object a, list o );
static list     skip_quote( object q, list o );
static list     nested_comment( list o );
static void     print( list o, FILE *f );

static object   Int( int i );
static list     cons( object a, object b );
static list     one( object a );
static object   car( list o );
static object   cdr( list o );
static int      eq( object a, object b );
static int      eqint( object a, int i );
static int      match( object pat, object it, object *matched, object *tail );

static object   add_global_root( object o, object *op );
static int      collect( object local_roots );
static void       mark( object ob );
static int        sweep( record **ptr );
static record *   alloc();
static object   new_( object a );
static int      error( char *msg );


record *allocation_list;
object global_roots;
list   slnl; // patterns
list   single,
       multi;
list   starsl;

int
main( int argc, char **argv ){
  init_patterns();
  list input = chars_from_file( argc > 1  ? fopen( argv[1], "r" )  : stdin );
  if(  debug_level >= 2  )
    fprintf( stderr, "input:\n"), print( input, stderr );

  list logical = logical_lines( input );
  if(  debug_level >= 2  )
    fprintf( stderr, "logical:\n"), print( logical, stderr );

  list stripped = strip_comments( logical );
  if(  debug_level >= 2  )
    fprintf( stderr, "stripped:\n"), print( stripped, stderr );

  list restored = restore_continues( stripped );
  if(  debug_level >= 2  )
    fprintf( stderr, "restored:\n");

  print( restored, stdout ), fflush( stdout );
  cleanup( restored ), input = logical = stripped = restored = NULL;
}

void
init_patterns(){
  slnl   = add_global_root( cons( Int( '\\' ), one( Int( '\n' ) ) ), &slnl );
  single = add_global_root( cons( Int( '/' ), one( Int( '/' ) ) ), &single );
  multi  = add_global_root( cons( Int( '/' ), one( Int( '*' ) ) ), &multi  );
  starsl = add_global_root( cons( Int( '*' ), one( Int( '/' ) ) ), &starsl ); 
}

void
cleanup( object restored ){
  if(  debug_level  ){
    if(  debug_level >= 2  ) fprintf( stderr, "@%d\n", collect( restored ) );
    if(  handle_roots  ){ global_roots = NULL; }
    fprintf( stderr, "@%d\n", collect( NULL ) );
  }
}

list
chars_from_file( FILE *f ){
  int c = fgetc( f );
  return  c != EOF  ? cons( Int( c ),
                            chars_from_file( f ) )
       :  one( Int( c ) );
}


list
logical_lines( list o ){
  if(  !o  ) return  NULL;
  if(  debug_level >= 2 && car(o)->Int.i != EOF  )
      fprintf( stderr, "[%c%c]", car(o)->Int.i, car(cdr(o))->Int.i );
  object matched, tail;
  return  match( slnl, o, &matched, &tail )  ? trim_continue( o, tail )
       :  cons( car( o ),
                logical_lines( cdr( o ) ) );
}

list
trim_continue( list o, list tail ){
  if(  debug_level >= 2  ) fprintf( stderr, "@" );
  return  car( tail )->Int.continues = car( o )->Int.continues + 1,
          logical_lines( tail );
}

list
restore_continues( list o ){
  return  !o  ? NULL
       :  car( o )->Int.continues-->0  ?
            cons( Int( '\\' ),
                  cons( Int( '\n' ),
                        restore_continues( o ) ) )
       :  cons( car( o ),
                restore_continues( cdr( o ) ) );
}

list
strip_comments( list o ){
  if(  !o  ) return  NULL;
  if(  debug_level >= 2 && car(o)->Int.i != EOF  )
      fprintf( stderr, "<%c%c>", car(o)->Int.i, car(cdr(o))->Int.i );
  object matched, tail;
  object a;
  return  match( single, o, &matched, &tail )  ? single_remainder( tail )
       :  match( multi, o, &matched, &tail )   ? multi_remainder( tail )
       :  starts_literal( a = car( o ) )       ? literal_val( a, cdr( o ) )
       :  cons( a,
                strip_comments( cdr( o ) ) );
}

list
single_remainder( list tail ){
  if(  debug_level >= 2  ) fprintf( stderr, "@/" );
  object c;
  for(  c = car( tail );
        tail && ! (eqint( c, '\n' ) || eqint( c, EOF ));
	c = car( tail = cdr( tail ) )  )
    ;
  return  eqint( c, '\n' )  ? cons( Int( '\n' ),
                                    strip_comments( cdr( tail ) ) )
       :  tail;
}

list
multi_remainder( list tail ){
  if(  debug_level >= 2  ) fprintf( stderr, "@*" );
  return  cons( Int( ' ' ),
                strip_comments( nested_comment( tail ) ) );
}

int
starts_literal( object a ){
  return  eqint( a, '\'' ) || eqint( a, '"' );
}

list
literal_val( object a, list o ){
  return  cons( a,
                skip_quote( a, o ) );
}

list
nested_comment( list o ){
  if(  !o  ) error( "Unterminated comment\n" );
  if(  debug_level >= 2  )
    fprintf( stderr, "(%c%c)", car( o )->Int.i, car( cdr( o ) )->Int.i );
  object matched, tail;
  return  match( starsl, o, &matched, &tail )  ? tail
       :  eqint( car( o ), EOF )               ? error( "Unterminated comment\n" ),NULL
       :  nested_comment( cdr( o ) );
}

list
skip_quote( object q, list o ){
  if(  !o  ) error( "Unterminated literal\n" );
  object a = car( o );
  return  eqint( a, '\\' )  ? cons( a, 
                                    cons( car( cdr( o ) ),
                                          skip_quote( q, cdr( cdr( o ) ) ) ) )
       :  eq( a, q )        ? cons( a,
                                    strip_comments( cdr( o ) ) )
       :  eqint( a, '\n' )
       || eqint( a, EOF )   ? error( "Unterminated literal\n" ),NULL
       :  cons( a,
                skip_quote( q, cdr( o ) ) );
}

void
print( list o, FILE *f ){
  switch(  o  ? o->t  : 0  ){
    case INTEGER: if(  o->Int.i != EOF  ) fputc( o->Int.i, f );
                  break;
    case LIST: print( car( o ), f );
               print( cdr( o ), f ); break;
  }
}


object
Int( int i ){
  return  OBJECT( .Int = { INTEGER, i } );
}

list
cons( object a, object b ){
  return  OBJECT( .List = { LIST, a, b } );
}

list
one( object a ){
  return  cons( a, NULL );
}

object
car( list o ){
  return  o && o->t == LIST  ? o->List.a  : NULL;
}

object
cdr( list o ){
  return  o && o->t == LIST  ? o->List.b  : NULL;
}

int
eq( object a, object b ){
  return  !a && !b         ? 1
       :  !a || !b         ? 0
       :  a->t != b->t     ? 0
       :  a->t == INTEGER  ? a->Int.i == b->Int.i
       :  !memcmp( a, b, sizeof *a );
}

int
eqint( object a, int i ){
  union uobject b = { .Int = { INTEGER, i } };
  return  eq( a, &b );
}

int
match( object pat, object it, object *matched, object *tail ){
  if(  !pat  ) return  *tail = it,  1;
  if(  pat->t != (it  ? it->t  : 0)  ) return  0;
  switch(  pat->t  ){
    case LIST: {
        object sink;
        if(  match( car( pat ), car( it ), & sink, tail )  )
          return  *matched = it,
                  match( cdr( pat ), cdr( it ), & sink, tail );
      } break;
    case INTEGER:
      if(  eq( pat, it )  ) return  *matched = it,  1;
  }
  return  0;
}


object
add_global_root( object o, object *op ){
  if(  handle_roots  ){
    global_roots = cons( o, global_roots );
    record *r = ((void*)( (char*)o - offsetof( record, o ) ) );
    r->handle = op;
  }
  return  o;
}

void
mark( object ob ){
  if(  !ob  ) return;
  record *r = ((void*)( (char*)ob - offsetof( record, o ) ) );
  if(  r->mark  ) return;
  r->mark = 1;
  switch(  ob  ? ob->t  : 0  ){
    case LIST: mark( ob->List.a ); mark( ob->List.b ); break;
  }
}

int
sweep( record **ptr ){
  int count = 0;
  while(  *ptr  ){
    if(  (*ptr)->mark  ){
      (*ptr)->mark = 0;
      ptr = &(*ptr)->prev;
    } else {
      record *z = *ptr;
      if(  z->handle  ) *z->handle = NULL;
      *ptr = (*ptr)->prev;
      free( z );
      ++count;
    }
  }
  return  count;
}

int
collect( object local_roots ){
  mark( local_roots );
  mark( global_roots );
  return  sweep( &allocation_list );
}

record *
alloc(){
  return  calloc( 1, sizeof(record) );
}

object
new_( object a ){
  record *r = alloc();
  object p = NULL;
  if(  r  ){
    r->prev = allocation_list;
    allocation_list = r;
    p = (void*)( ((char*)r) + offsetof( record, o ) );
    *p = *a;
  }
  return  p;
}

int
error( char *msg ){
  fprintf( stderr, "%s", msg );
  exit( EXIT_FAILURE );
  return 0777;
}
