//#define DEBUG(...) __VA_ARGS__
#define DEBUG(...)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union uobject *object;
typedef object fSuspension( object );
typedef object list;
typedef enum tag { INVALID, INTEGER, LIST, SUSPENSION, VOID } tag;

union uobject { tag t;
       struct { tag t; int i; } Int;
       struct { tag t; object a, b; } List;
       struct { tag t; object v; fSuspension *f; } Suspension;
       struct { tag t; void *v; } Void;
};


list skip_quote( list o );
list nested_comment( list o );
void print( list o );


object add_global_root( object o ){ return  o; }
object alloc(){
  return  calloc( 1, sizeof(object) );
}

#define OBJECT(...) new_( (union uobject[]){{ __VA_ARGS__ }} )
object new_( object a ){
  object p = alloc();
  return  p  ? *p = *a, p  : 0;
}


object Int( int i ){ return  OBJECT( .Int = { INTEGER, i } ); }

list cons( object a, object b ){ return  OBJECT( .List = { LIST, a, b } ); }
list one( object a ){ return  cons( a, NULL ); }
object car( list o ){ return  o  && o->t == LIST  ? o->List.a  : NULL; }
object cdr( list o ){ return  o  && o->t == LIST  ? o->List.b  : NULL; }

object Void( void *v ){ return  OBJECT( .Void = { VOID, v } ); }

object Suspension( void *v, fSuspension *f ){
  return  OBJECT( .Suspension = { SUSPENSION, v, f } );
}

int eq( object a, object b ){
  return  !a && !b                    ? 1  :
          !a || !b                    ? 0  :
          a->t != b->t                ? 0  :
          !memcmp( a, b, sizeof *a );
}

int eqint( object a, int i ){
  union uobject b = { .Int = { INTEGER, i } };
  return  eq( a, &b );
}

object force( object o ){
  return  !o || o->t != SUSPENSION  ? o  :
            force( o->Suspension.f( o->Suspension.v ) );
}

list take( int i, list o ){
  return  i == 0  ? NULL  :
          !o      ? NULL  :
          ( *o = *force( o ),
            cons( car( o ), take( i-1, cdr( o ) ) ) );
}
list drop( int i, list o ){
  return  i == 0  ? o  :
          !o      ? NULL  :
          ( *o = *force( o ),
            drop( i-1, cdr( o ) ) );
}

object first( list o ){ return  car( take( 1, o ) ); }
object rest( list o ){ return  drop( 1, o ); }

int match( object pat, object it, object *matched, object *tail ){
  if(  !pat  ){
    *tail = it;
    return  1;
  }
  if(  (it  ? it->t  : 0) == SUSPENSION  ) *it = *force( it );
  if(  pat->t != (it  ? it->t  : 0)  ) return  0;
  switch(  pat->t  ){
    default: return  0;
    case LIST: {
        object sink;
        if(  match( first( pat ), first( it ), & sink, tail )  ){
	  *matched = it;
          return  match( rest( pat ), rest( it ), & sink, tail );
        } 
      } break;
    case INTEGER:
      if(  eq( pat, it )  ){
	*matched = it;
        return  1;
      } 
  }
  return  0;
}

static list
force_chars_from_file( object file ){
  FILE *f = file->Void.v;
  int c = fgetc( f );
  return  c != EOF  ? cons( Int( c ), Suspension( file, force_chars_from_file ) )
                    : cons( Int( EOF ), NULL );
}

list chars_from_file( FILE *f ){
  return  f  ? Suspension( Void( f ), force_chars_from_file )  : NULL;
}

list logical_lines( list o ){
  DEBUG( fprintf( stderr, "[%c%c]", first( o )->Int.i, first( rest( o ) )->Int.i ); )
  static list pat;
  if(  !pat  ) pat = add_global_root( cons( Int( '\\' ), one( Int( '\n' ) ) ) );

  object matched, tail;
  if(  match( pat, o, &matched, &tail )  ){
    return  Suspension( tail, logical_lines );
  } else {
    object a = first( o );
    return  eqint( a, EOF )  ? o  : cons( a, Suspension( rest( o ), logical_lines ) );
  }
}

list strip_comments( list o ){
  DEBUG( fprintf( stderr, "<%c%c>", first( o )->Int.i, first( rest( o ) )->Int.i ); )
  static list single, multi;
  if(  !single  ) single = add_global_root( cons( Int( '/' ), one( Int( '/' ) ) ) );
  if(  !multi   ) multi = add_global_root( cons( Int( '/' ), one( Int( '*' ) ) ) );

  object matched, tail;
  if(  match( single, o, &matched, &tail )  ){
    do {
      tail = rest( tail );
      matched = first( tail );
      if(  eqint( matched, '\\'  )  ) tail = rest( tail );  // eat \NL
    } while(  !eqint( matched, '\n' )  );
    return  Suspension( tail, strip_comments );
  } else if(  match( multi, o, &matched, &tail )  ){
    return  cons( Int( ' ' ), strip_comments( nested_comment( rest( tail ) ) ) );
  }

  object a = first( o );
  if(  eqint( a, '\'' ) || eqint( a, '"' )  ) return  cons( a, skip_quote( o ) );
  return  eqint( a, EOF )  ? o  : cons( a, Suspension( rest( o ), strip_comments ) );
}

list nested_comment( list o ){
  DEBUG( fprintf( stderr, "(%c%c)", first( o )->Int.i, first( rest( o ) )->Int.i ); )
  static list end;
  if(  !end  ) end = add_global_root( cons( Int( '*' ), one( Int( '/' ) ) ) ); 

  object matched, tail;
  if(  match( end, o, &matched, &tail )  ) return  tail;
  if(  eqint( car( o ), EOF )  ) fprintf( stderr, "Unterminated comment\n"), exit( 1 );
  return  nested_comment( rest( o ) );
}

// quote character q is "curried" onto the front of the list
list skip_quote( list o ){
  object q = car( o );
  o = cdr( o );
  object a = first( o );
  if(  eqint( a, '\\' )  ){
    return  cons( a, cons( first( rest( o ) ), skip_quote( cons( q, drop( 2, o ) ) ) ) );
  } else if(  eq( a, q )  ){
    return  cons( a, strip_comments( rest( o ) ) );
  }
  if(  eqint( a, EOF )  ) fprintf( stderr, "Unterminated literal\n"), exit( 1 );
  return  cons( a, skip_quote( cons( q, rest( o ) ) ) );
}

void print( list o ){
  switch(  o  ? o->t  : 0  ){
    default: break;
    case INTEGER: if(  o->Int.i != EOF  ) fputc( o->Int.i, stdout ); break;
    case SUSPENSION:
    case LIST: print( first( o ) );
               print( rest( o ) ); break;
  }
}


int main(){
  list input = chars_from_file( stdin );
  print( strip_comments( input ) );
  //print( strip_comments( logical_lines( input ) ) );
}
