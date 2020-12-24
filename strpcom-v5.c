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


record *allocation_list;
object global_roots;

static list     chars_from_file( FILE *f );
static list     logical_lines( list o );
static list     restore_continues( list o );
static list     strip_comments( list o );
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
static void     error( char *msg );


int
main( int argc, char **argv ){
  list input = chars_from_file( argc > 1 ? fopen( argv[1], "r" )  : stdin );
  if(  debug_level >= 2  ) fprintf( stderr, "input:\n"), print( input, stderr );

  list logical = logical_lines( input );
  if(  debug_level >= 2  ) fprintf( stderr, "logical:\n"), print( logical, stderr );

  list stripped = strip_comments( logical );
  if(  debug_level >= 2  ) fprintf( stderr, "stripped:\n"), print( stripped, stderr );

  list restored = restore_continues( stripped );
  if(  debug_level >= 2  ) fprintf( stderr, "restored:\n");

  print( restored, stderr );

  if(  debug_level >= 2  ) fprintf( stderr, "@%d\n", collect( restored ) );
  if(  debug_level  ){
    if(  handle_roots  ){ global_roots = NULL; }
    fprintf( stderr, "@%d\n", collect( NULL ) );
    input = logical = stripped = restored = NULL;
  }
}

list
chars_from_file( FILE *f ){
  int c = fgetc( f );
  return  c != EOF  ? cons( Int( c ), chars_from_file( f ) )
                    : one( Int( c ) );
}

list
logical_lines( list o ){
  if(  !o  ) return  NULL;
  static list pat;
  if(  !pat  )
    pat = add_global_root( cons( Int( '\\' ), one( Int( '\n' ) ) ), &pat );

  object matched, tail;
  if(  debug_level >= 2  )
    if( car(o)->Int.i!=EOF )
      fprintf( stderr, "[%c%c]", car(o)->Int.i, car(cdr(o))->Int.i );
  if(  match( pat, o, &matched, &tail )  ){
    if(  debug_level >= 2  ) fprintf( stderr, "@" );
    car( tail )->Int.continues = car( o )->Int.continues + 1;
    return  logical_lines( tail );
  } else {
    object a = car( o );
    return  cons( a, logical_lines( cdr( o ) ) );
  }
}

list
restore_continues( list o ){
  if(  !o  ) return  NULL;
  object a = car( o );
  object z = cdr( o );
  object r = cons( a, restore_continues( z ) );
  while(  a->Int.continues  ){
    r = cons( Int( '\\' ), cons( Int( '\n' ), r ) );
    --a->Int.continues;
  }
  return  r;
}

list
strip_comments( list o ){
  if(  !o  ) return  NULL;
  static list single, multi;
  if(  !single  )
    single = add_global_root( cons( Int('/'), one(Int('/') ) ), &single );
  if(  !multi   )
    multi = add_global_root( cons( Int('/'), one(Int('*') ) ), &multi );

  object matched, tail;
  if(  debug_level >= 2 && car(o)->Int.i != EOF  )
      fprintf( stderr, "<%c%c>", car(o)->Int.i, car(cdr(o))->Int.i );
  if(  match( single, o, &matched, &tail )  ){
    if(  debug_level >= 2  ) fprintf( stderr, "@" );
    object c;
    for(  c = car( tail );
          tail && ! (eqint( c, '\n' ) || eqint( c, EOF ));
	  c = car( tail = cdr( tail ) )  )
      ;
    return  cons( Int( '\n' ),
                  strip_comments( cdr( tail ) ) );
  } else if(  match( multi, o, &matched, &tail )  ){
    if(  debug_level >= 2  ) fprintf( stderr, "@" );
    return  cons( Int( ' ' ),
                  strip_comments( nested_comment( cdr( tail ) ) ) );
  }

  object a = car( o );
  if(  eqint( a, '\'' ) || eqint( a, '"' )  )
    return  cons( a,
                  skip_quote( a, cdr( o ) ) );
  return  cons( a,
                strip_comments( cdr( o ) ) );
}

list
nested_comment( list o ){
  if(  !o  ) error( "Unterminated comment\n" );
  if(  debug_level >= 2  )
    fprintf( stderr, "(%c%c)", car( o )->Int.i, car( cdr( o ) )->Int.i );
  static list end;
  if(  !end  )
    end = add_global_root( cons( Int( '*' ), one( Int( '/' ) ) ), &end ); 

  object matched, tail;
  if(  match( end, o, &matched, &tail )  ) return  tail;
  if(  eqint( car( o ), EOF )  ) error( "Unterminated comment\n" );
  return  nested_comment( cdr( o ) );
}

list
skip_quote( object q, list o ){
  if(  !o  ) error( "Unterminated literal\n" );
  object a = car( o );
  if(  eqint( a, '\\' )  )
    return  cons( a,
                  cons( car( cdr( o ) ),
                        skip_quote( q, cdr( cdr( o ) ) ) ) );
  else if(  eq( a, q )  )
    return  cons( a,
                  strip_comments( cdr( o ) ) );
  if(  eqint( a, EOF )  ) error( "Unterminated literal\n" );
  return  cons( a,
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
  return  o  && o->t == LIST  ? o->List.a  : NULL;
}

object
cdr( list o ){
  return  o  && o->t == LIST  ? o->List.b  : NULL;
}

int
eq( object a, object b ){
  return  !a && !b         ? 1                     :
          !a || !b         ? 0                     :
          a->t != b->t     ? 0                     :
          a->t == INTEGER  ? a->Int.i == b->Int.i  :
          !memcmp( a, b, sizeof *a );
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
    case LIST:
      {
        object sink;
        if(  match( car( pat ), car( it ), & sink, tail )  ){
          return  *matched = it,
                  match( cdr( pat ), cdr( it ), & sink, tail );
        } 
      } break;
    case INTEGER:
      if(  eq( pat, it )  ){
        return  *matched = it,  1;
      } 
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

void
error( char *msg ){
  fprintf( stderr, "%s", msg );
  exit( EXIT_FAILURE );
}
