//strpcom-v5.c:
//$ make strpcom-v5 CFLAGS='-std=c99 -Wall -pedantic -Wextra -Wno-switch'
#define DEBUG(...) __VA_ARGS__
//#define DEBUG(...)
#define HANDLE_ROOTS(...) __VA_ARGS__
//#define HANDLE_ROOTS(...)
#define FINAL_GC(...) __VA_ARGS__
//#define FINAL_GC(...)

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union uobject *object;
typedef object list;
typedef enum tag { INVALID, INTEGER, LIST } tag;

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


list cons( object a, object b );
list skip_quote( object q, list o );
list nested_comment( list o );
void print( list o );


record *allocation_list;
object global_roots;
object add_global_root( object o, object *op ){
  HANDLE_ROOTS(
                global_roots = cons( o, global_roots );
		record *r = ((void*)( (char*)o - offsetof( record, o ) ) );
		r->handle = op;
              )
  return  o;
}
record *alloc(){
  return  calloc( 1, sizeof(record) );
}
void mark( object ob ){
  if(  !ob  ) return;
  record *r = ((void*)( (char*)ob - offsetof( record, o ) ) );
  if(  r->mark  ) return;
  r->mark = 1;
  switch(  ob  ? ob->t  : 0  ){
    case LIST: mark( ob->List.a ); mark( ob->List.b ); break;
  }
}
int sweep( record **ptr ){
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
int collect( object local_roots ){
  mark( local_roots );
  mark( global_roots );
  return  sweep( &allocation_list );
}

#define OBJECT(...) new_( (union uobject[]){{ __VA_ARGS__ }} )
object new_( object a ){
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

object Int( int i ){ return  OBJECT( .Int = { INTEGER, i } ); }
list cons( object a, object b ){ return  OBJECT( .List = { LIST, a, b } ); }
list one( object a ){ return  cons( a, NULL ); }
object car( list o ){ return  o  && o->t == LIST  ? o->List.a  : NULL; }
object cdr( list o ){ return  o  && o->t == LIST  ? o->List.b  : NULL; }

int eq( object a, object b ){
  return  !a && !b                    ? 1  :
          !a || !b                    ? 0  :
          a->t != b->t                ? 0  :
          a->t == INTEGER  ? a->Int.i == b->Int.i
                           : !memcmp( a, b, sizeof *a );
}

int eqint( object a, int i ){
  union uobject b = { .Int = { INTEGER, i } };
  return  eq( a, &b );
}

int match( object pat, object it, object *matched, object *tail ){
  if(  !pat  ){
    return  *tail = it,  1;
  }
  if(  pat->t != (it  ? it->t  : 0)  ) return  0;
  switch(  pat->t  ){
    case LIST:
      {
        object sink;
        if(  match( car( pat ), car( it ), & sink, tail )  ){
          return  *matched = it,  match( cdr( pat ), cdr( it ), & sink, tail );
        } 
      } break;
    case INTEGER:
      if(  eq( pat, it )  ){
        return  *matched = it,  1;
      } 
  }
  return  0;
}

list chars_from_file( FILE *f ){
  int c = fgetc( f );
  return  c != EOF  ? cons( Int( c ), chars_from_file( f ) )  : one( Int( c ) );
}

list logical_lines( list o ){
  static list pat;
  if(  !pat  ) pat = add_global_root( cons( Int( '\\' ), one( Int( '\n' ) ) ), &pat );

  object matched, tail;
  DEBUG( if( car(o)->Int.i!=EOF )
         fprintf( stderr, "[%c%c]", car(o)->Int.i, car(cdr(o))->Int.i ); )
  if(  match( pat, o, &matched, &tail )  ){
    DEBUG( fprintf( stderr, "@" ); )
    car( tail )->Int.continues = car( o )->Int.continues + 1;
    return  logical_lines( tail );
  } else {
    object a = car( o );
    return  eqint( a, EOF )  ? o  : cons( a, logical_lines( cdr( o ) ) );
  }
}

list restore_continues( list o ){
  if(  !o  ) return  NULL;
  object a = car( o );
  if(  eqint( a, EOF )  ) return  o;
  object z = cdr( o );
  object r = cons( a, restore_continues( z ) );
  while(  a->Int.continues  ){
    r = cons( Int( '\\' ), cons( Int( '\n' ), r ) );
    --a->Int.continues;
  }
  return  r;
}

list strip_comments( list o ){
  static list single, multi;
  if(  !single  ) single = add_global_root( cons( Int('/'), one(Int('/')) ), &single );
  if(  !multi   ) multi = add_global_root( cons( Int('/'), one(Int('*')) ), &multi );

  object matched, tail;
  DEBUG( if(car(o)->Int.i!=EOF)
          fprintf(stderr,"<%c%c>", car(o)->Int.i, car(cdr(o))->Int.i); )
  if(  match( single, o, &matched, &tail )  ){
    DEBUG( fprintf( stderr, "@" ); )
    for(  matched = car( tail ), tail = cdr( tail );
          !eqint( matched, '\n' ) && !eqint( matched, EOF );
	  matched = car( tail ), tail = cdr( tail )  )
      ;
    return  strip_comments( tail );
  } else if(  match( multi, o, &matched, &tail )  ){
    DEBUG( fprintf( stderr, "@" ); )
    return  cons( Int( ' ' ),
                  strip_comments( nested_comment( cdr( tail ) ) ) );
  }

  object a = car( o );
  if(  eqint( a, '\'' ) || eqint( a, '"' )  )
    return  cons( a, skip_quote( a, cdr( o ) ) );
  return  eqint( a, EOF )  ? o
                           : cons( a, strip_comments( cdr( o ) ) );
}

list nested_comment( list o ){
  DEBUG(fprintf( stderr, "(%c%c)", car( o )->Int.i, car( cdr( o ) )->Int.i );)
  static list end;
  if(  !end  ) end = add_global_root( cons( Int( '*' ), one( Int( '/' ) ) ), &end ); 

  object matched, tail;
  if(  match( end, o, &matched, &tail )  )
    return  tail;
  if(  eqint( car( o ), EOF )  )
    fprintf( stderr, "Unterminated comment\n"),
    exit( 1 );
  return  nested_comment( cdr( o ) );
}

list skip_quote( object q, list o ){
  object a = car( o );
  if(  eqint( a, '\\' )  ){
    return  cons( a, cons( car( cdr( o ) ),
                           skip_quote( q, cdr( cdr( o ) ) ) ) );
  } else if(  eq( a, q )  ){
    return  cons( a, strip_comments( cdr( o ) ) );
  }
  if(  eqint( a, EOF )  )
    fprintf( stderr, "Unterminated literal\n"),
    exit( 1 );
  return  cons( a, skip_quote( q, cdr( o ) ) );
}

void print( list o ){
  switch(  o  ? o->t  : 0  ){
    case INTEGER: if(  o->Int.i != EOF  ) fputc( o->Int.i, stdout );
                  break;
    case LIST: print( car( o ) );
               print( cdr( o ) ); break;
  }
}


int main( int argc, char **argv ){
  list input = chars_from_file( argc > 1 ? fopen( argv[1], "r" )  : stdin );
  DEBUG( printf( "input:\n"); print( input ); )
  list logical = logical_lines( input );
  DEBUG( printf( "logical:\n"); print( logical ); )
  list stripped = strip_comments( logical );
  DEBUG( printf( "stripped:\n"); print( stripped ); )
  list restored = restore_continues( stripped );
  DEBUG( printf( "restored:\n"); )
  print( restored );
  DEBUG( printf( "@%d\n", collect( restored ) ); )
  FINAL_GC(
	 HANDLE_ROOTS( global_roots = NULL; )
         printf( "@%d\n", collect( NULL ) );
         input = logical = stripped = restored = NULL;
         )
}
