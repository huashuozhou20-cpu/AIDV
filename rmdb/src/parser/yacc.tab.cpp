/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"

#include "ast.h"
#include "yacc.tab.h"
#include <iostream>
#include <memory>

int yylex(YYSTYPE *yylval, YYLTYPE *yylloc);

void yyerror(YYLTYPE *locp, const char* s) {
    std::cerr << "Parser Error at line " << locp->first_line << " column " << locp->first_column << ": " << s << std::endl;
}

using namespace ast;

#line 86 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "yacc.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_SHOW = 3,                       /* SHOW  */
  YYSYMBOL_TABLES = 4,                     /* TABLES  */
  YYSYMBOL_CREATE = 5,                     /* CREATE  */
  YYSYMBOL_TABLE = 6,                      /* TABLE  */
  YYSYMBOL_DROP = 7,                       /* DROP  */
  YYSYMBOL_DESC = 8,                       /* DESC  */
  YYSYMBOL_INSERT = 9,                     /* INSERT  */
  YYSYMBOL_INTO = 10,                      /* INTO  */
  YYSYMBOL_VALUES = 11,                    /* VALUES  */
  YYSYMBOL_DELETE = 12,                    /* DELETE  */
  YYSYMBOL_FROM = 13,                      /* FROM  */
  YYSYMBOL_ASC = 14,                       /* ASC  */
  YYSYMBOL_ORDER = 15,                     /* ORDER  */
  YYSYMBOL_BY = 16,                        /* BY  */
  YYSYMBOL_WHERE = 17,                     /* WHERE  */
  YYSYMBOL_UPDATE = 18,                    /* UPDATE  */
  YYSYMBOL_SET = 19,                       /* SET  */
  YYSYMBOL_SELECT = 20,                    /* SELECT  */
  YYSYMBOL_INT = 21,                       /* INT  */
  YYSYMBOL_CHAR = 22,                      /* CHAR  */
  YYSYMBOL_FLOAT = 23,                     /* FLOAT  */
  YYSYMBOL_INDEX = 24,                     /* INDEX  */
  YYSYMBOL_AND = 25,                       /* AND  */
  YYSYMBOL_JOIN = 26,                      /* JOIN  */
  YYSYMBOL_EXIT = 27,                      /* EXIT  */
  YYSYMBOL_HELP = 28,                      /* HELP  */
  YYSYMBOL_DEFAULT = 29,                   /* DEFAULT  */
  YYSYMBOL_PRIMARY = 30,                   /* PRIMARY  */
  YYSYMBOL_KEY = 31,                       /* KEY  */
  YYSYMBOL_AUTO_INCREMENT = 32,            /* AUTO_INCREMENT  */
  YYSYMBOL_AS = 33,                        /* AS  */
  YYSYMBOL_CONCAT = 34,                    /* CONCAT  */
  YYSYMBOL_VIEW = 35,                      /* VIEW  */
  YYSYMBOL_UNIQUE = 36,                    /* UNIQUE  */
  YYSYMBOL_DATE = 37,                      /* DATE  */
  YYSYMBOL_TEXT = 38,                      /* TEXT  */
  YYSYMBOL_TXN_BEGIN = 39,                 /* TXN_BEGIN  */
  YYSYMBOL_TXN_COMMIT = 40,                /* TXN_COMMIT  */
  YYSYMBOL_TXN_ABORT = 41,                 /* TXN_ABORT  */
  YYSYMBOL_TXN_ROLLBACK = 42,              /* TXN_ROLLBACK  */
  YYSYMBOL_ENABLE_NESTLOOP = 43,           /* ENABLE_NESTLOOP  */
  YYSYMBOL_ENABLE_SORTMERGE = 44,          /* ENABLE_SORTMERGE  */
  YYSYMBOL_AVG = 45,                       /* AVG  */
  YYSYMBOL_BETWEEN = 46,                   /* BETWEEN  */
  YYSYMBOL_COUNT = 47,                     /* COUNT  */
  YYSYMBOL_DISTINCT = 48,                  /* DISTINCT  */
  YYSYMBOL_FULL = 49,                      /* FULL  */
  YYSYMBOL_FOR = 50,                       /* FOR  */
  YYSYMBOL_GROUP = 51,                     /* GROUP  */
  YYSYMBOL_HAVING = 52,                    /* HAVING  */
  YYSYMBOL_IN = 53,                        /* IN  */
  YYSYMBOL_INNER = 54,                     /* INNER  */
  YYSYMBOL_IS = 55,                        /* IS  */
  YYSYMBOL_LEFT = 56,                      /* LEFT  */
  YYSYMBOL_LIKE = 57,                      /* LIKE  */
  YYSYMBOL_LIMIT = 58,                     /* LIMIT  */
  YYSYMBOL_MAX = 59,                       /* MAX  */
  YYSYMBOL_MIN = 60,                       /* MIN  */
  YYSYMBOL_NOT = 61,                       /* NOT  */
  YYSYMBOL_OFFSET = 62,                    /* OFFSET  */
  YYSYMBOL_ON = 63,                        /* ON  */
  YYSYMBOL_OR = 64,                        /* OR  */
  YYSYMBOL_RIGHT = 65,                     /* RIGHT  */
  YYSYMBOL_SUM = 66,                       /* SUM  */
  YYSYMBOL_LEQ = 67,                       /* LEQ  */
  YYSYMBOL_NEQ = 68,                       /* NEQ  */
  YYSYMBOL_GEQ = 69,                       /* GEQ  */
  YYSYMBOL_T_EOF = 70,                     /* T_EOF  */
  YYSYMBOL_IDENTIFIER = 71,                /* IDENTIFIER  */
  YYSYMBOL_VALUE_STRING = 72,              /* VALUE_STRING  */
  YYSYMBOL_VALUE_INT = 73,                 /* VALUE_INT  */
  YYSYMBOL_VALUE_FLOAT = 74,               /* VALUE_FLOAT  */
  YYSYMBOL_VALUE_BOOL = 75,                /* VALUE_BOOL  */
  YYSYMBOL_VALUE_NULL = 76,                /* VALUE_NULL  */
  YYSYMBOL_77_ = 77,                       /* ';'  */
  YYSYMBOL_78_ = 78,                       /* '='  */
  YYSYMBOL_79_ = 79,                       /* '('  */
  YYSYMBOL_80_ = 80,                       /* ')'  */
  YYSYMBOL_81_ = 81,                       /* ','  */
  YYSYMBOL_82_ = 82,                       /* '+'  */
  YYSYMBOL_83_ = 83,                       /* '-'  */
  YYSYMBOL_84_ = 84,                       /* '*'  */
  YYSYMBOL_85_ = 85,                       /* '/'  */
  YYSYMBOL_86_ = 86,                       /* '%'  */
  YYSYMBOL_87_ = 87,                       /* '.'  */
  YYSYMBOL_88_ = 88,                       /* '<'  */
  YYSYMBOL_89_ = 89,                       /* '>'  */
  YYSYMBOL_YYACCEPT = 90,                  /* $accept  */
  YYSYMBOL_start = 91,                     /* start  */
  YYSYMBOL_stmt = 92,                      /* stmt  */
  YYSYMBOL_txnStmt = 93,                   /* txnStmt  */
  YYSYMBOL_dbStmt = 94,                    /* dbStmt  */
  YYSYMBOL_setStmt = 95,                   /* setStmt  */
  YYSYMBOL_ddl = 96,                       /* ddl  */
  YYSYMBOL_dml = 97,                       /* dml  */
  YYSYMBOL_fieldList = 98,                 /* fieldList  */
  YYSYMBOL_colNameList = 99,               /* colNameList  */
  YYSYMBOL_optColList = 100,               /* optColList  */
  YYSYMBOL_field = 101,                    /* field  */
  YYSYMBOL_type = 102,                     /* type  */
  YYSYMBOL_valueList = 103,                /* valueList  */
  YYSYMBOL_valueTupleList = 104,           /* valueTupleList  */
  YYSYMBOL_value = 105,                    /* value  */
  YYSYMBOL_condition = 106,                /* condition  */
  YYSYMBOL_cond_or = 107,                  /* cond_or  */
  YYSYMBOL_cond_and = 108,                 /* cond_and  */
  YYSYMBOL_cond_not = 109,                 /* cond_not  */
  YYSYMBOL_cond_base = 110,                /* cond_base  */
  YYSYMBOL_optWhereClause = 111,           /* optWhereClause  */
  YYSYMBOL_optHaving = 112,                /* optHaving  */
  YYSYMBOL_expr = 113,                     /* expr  */
  YYSYMBOL_expr_add_sub = 114,             /* expr_add_sub  */
  YYSYMBOL_expr_mul_div = 115,             /* expr_mul_div  */
  YYSYMBOL_expr_unary = 116,               /* expr_unary  */
  YYSYMBOL_expr_base = 117,                /* expr_base  */
  YYSYMBOL_aggType = 118,                  /* aggType  */
  YYSYMBOL_aggExpr = 119,                  /* aggExpr  */
  YYSYMBOL_optDistinct = 120,              /* optDistinct  */
  YYSYMBOL_optForUpdateClause = 121,       /* optForUpdateClause  */
  YYSYMBOL_selectItems = 122,              /* selectItems  */
  YYSYMBOL_selectItemList = 123,           /* selectItemList  */
  YYSYMBOL_opt_alias = 124,                /* opt_alias  */
  YYSYMBOL_optAliasStr = 125,              /* optAliasStr  */
  YYSYMBOL_fromList = 126,                 /* fromList  */
  YYSYMBOL_optJoinList = 127,              /* optJoinList  */
  YYSYMBOL_joinList = 128,                 /* joinList  */
  YYSYMBOL_joinType = 129,                 /* joinType  */
  YYSYMBOL_joinClause = 130,               /* joinClause  */
  YYSYMBOL_optGroupBy = 131,               /* optGroupBy  */
  YYSYMBOL_optLimit = 132,                 /* optLimit  */
  YYSYMBOL_col = 133,                      /* col  */
  YYSYMBOL_colList = 134,                  /* colList  */
  YYSYMBOL_op = 135,                       /* op  */
  YYSYMBOL_setClauses = 136,               /* setClauses  */
  YYSYMBOL_setClause = 137,                /* setClause  */
  YYSYMBOL_opt_order_clause = 138,         /* opt_order_clause  */
  YYSYMBOL_order_clause = 139,             /* order_clause  */
  YYSYMBOL_opt_asc_desc = 140,             /* opt_asc_desc  */
  YYSYMBOL_set_knob_type = 141,            /* set_knob_type  */
  YYSYMBOL_expr_list = 142,                /* expr_list  */
  YYSYMBOL_tbName = 143,                   /* tbName  */
  YYSYMBOL_colName = 144                   /* colName  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  42
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   389

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  90
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  55
/* YYNRULES -- Number of rules.  */
#define YYNRULES  175
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  324

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   331


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    86,     2,     2,
      79,    80,    84,    82,    81,    83,    87,    85,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    77,
      88,    78,    89,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    79,    79,    84,    89,    94,   102,   103,   104,   105,
     106,   110,   114,   118,   122,   129,   136,   143,   147,   151,
     155,   159,   164,   168,   172,   176,   183,   187,   191,   195,
     232,   236,   243,   247,   255,   258,   265,   269,   273,   277,
     281,   285,   291,   297,   303,   313,   317,   322,   327,   331,
     335,   339,   347,   351,   358,   362,   369,   373,   377,   381,
     385,   397,   401,   402,   410,   411,   419,   420,   428,   432,
     436,   440,   444,   448,   452,   456,   460,   464,   468,   472,
     480,   483,   491,   494,   505,   509,   510,   514,   521,   522,
     526,   530,   537,   538,   545,   549,   553,   557,   561,   565,
     576,   577,   578,   579,   580,   584,   588,   604,   605,   609,
     610,   614,   618,   622,   626,   633,   634,   635,   636,   637,
     638,   639,   640,   644,   645,   646,   647,   648,   649,   650,
     651,   655,   660,   669,   672,   679,   683,   690,   691,   692,
     693,   697,   701,   705,   709,   717,   720,   728,   731,   735,
     742,   746,   753,   757,   764,   768,   772,   776,   780,   784,
     791,   795,   802,   809,   813,   817,   821,   828,   829,   830,
     834,   835,   839,   843,   849,   851
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "SHOW", "TABLES",
  "CREATE", "TABLE", "DROP", "DESC", "INSERT", "INTO", "VALUES", "DELETE",
  "FROM", "ASC", "ORDER", "BY", "WHERE", "UPDATE", "SET", "SELECT", "INT",
  "CHAR", "FLOAT", "INDEX", "AND", "JOIN", "EXIT", "HELP", "DEFAULT",
  "PRIMARY", "KEY", "AUTO_INCREMENT", "AS", "CONCAT", "VIEW", "UNIQUE",
  "DATE", "TEXT", "TXN_BEGIN", "TXN_COMMIT", "TXN_ABORT", "TXN_ROLLBACK",
  "ENABLE_NESTLOOP", "ENABLE_SORTMERGE", "AVG", "BETWEEN", "COUNT",
  "DISTINCT", "FULL", "FOR", "GROUP", "HAVING", "IN", "INNER", "IS",
  "LEFT", "LIKE", "LIMIT", "MAX", "MIN", "NOT", "OFFSET", "ON", "OR",
  "RIGHT", "SUM", "LEQ", "NEQ", "GEQ", "T_EOF", "IDENTIFIER",
  "VALUE_STRING", "VALUE_INT", "VALUE_FLOAT", "VALUE_BOOL", "VALUE_NULL",
  "';'", "'='", "'('", "')'", "','", "'+'", "'-'", "'*'", "'/'", "'%'",
  "'.'", "'<'", "'>'", "$accept", "start", "stmt", "txnStmt", "dbStmt",
  "setStmt", "ddl", "dml", "fieldList", "colNameList", "optColList",
  "field", "type", "valueList", "valueTupleList", "value", "condition",
  "cond_or", "cond_and", "cond_not", "cond_base", "optWhereClause",
  "optHaving", "expr", "expr_add_sub", "expr_mul_div", "expr_unary",
  "expr_base", "aggType", "aggExpr", "optDistinct", "optForUpdateClause",
  "selectItems", "selectItemList", "opt_alias", "optAliasStr", "fromList",
  "optJoinList", "joinList", "joinType", "joinClause", "optGroupBy",
  "optLimit", "col", "colList", "op", "setClauses", "setClause",
  "opt_order_clause", "order_clause", "opt_asc_desc", "set_knob_type",
  "expr_list", "tbName", "colName", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-152)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-175)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     276,     7,    63,    61,   -55,     8,    30,   -55,    71,    13,
    -152,  -152,  -152,  -152,  -152,  -152,  -152,    76,    -6,  -152,
    -152,  -152,  -152,  -152,  -152,   -55,   -55,   -55,    69,   -55,
     -55,   -55,  -152,  -152,   -55,   -55,    81,  -152,  -152,     2,
    -152,   253,  -152,  -152,    33,   -49,    98,   -55,  -152,    70,
    -152,    73,    92,    83,    84,    89,  -152,  -152,  -152,  -152,
    -152,    74,  -152,  -152,  -152,  -152,  -152,   199,   306,  -152,
    -152,     1,    38,    88,  -152,  -152,    96,  -152,   150,    99,
    -152,    90,  -152,    83,   -55,    83,   242,   102,    83,    83,
     176,   288,  -152,  -152,    -8,  -152,   117,  -152,   306,   130,
     132,  -152,    82,  -152,  -152,   306,   306,   306,   306,   306,
      40,   -55,   306,    83,    56,  -152,   203,   139,   111,  -152,
    -152,   -55,   120,   124,   141,   288,   110,  -152,   167,   212,
    -152,  -152,    78,   192,    83,  -152,   217,  -152,   126,  -152,
    -152,  -152,  -152,  -152,  -152,  -152,  -152,    88,    88,  -152,
    -152,  -152,   168,   170,     3,     6,     1,  -152,  -152,    83,
    -152,   173,   178,  -152,  -152,    21,    83,  -152,    83,   182,
    -152,  -152,   217,   205,  -152,   183,   154,   288,   288,  -152,
    -152,  -152,  -152,  -152,  -152,   306,   217,   201,    -1,   225,
      86,  -152,  -152,  -152,   306,  -152,  -152,   -55,  -152,  -152,
    -152,  -152,   -55,    92,     9,   273,  -152,   143,  -152,  -152,
    -152,  -152,   228,   229,   217,   277,  -152,   231,  -152,   147,
    -152,    83,   155,  -152,   230,  -152,   212,  -152,  -152,   285,
      29,   235,  -152,  -152,   217,   241,   249,  -152,     6,     6,
     263,  -152,   -55,  -152,  -152,  -152,  -152,  -152,  -152,   243,
     250,  -152,   298,   -12,  -152,   175,  -152,   217,   217,   217,
     254,   186,  -152,   313,    29,  -152,   278,  -152,   323,   290,
       6,  -152,   270,  -152,   217,  -152,  -152,  -152,   188,  -152,
    -152,  -152,   217,   264,   196,   288,   274,   288,   335,   289,
     275,  -152,  -152,  -152,  -152,  -152,  -152,  -152,   287,  -152,
     340,   299,   288,  -152,   274,   274,   296,   308,  -152,  -152,
      11,   292,   312,   352,  -152,  -152,  -152,  -152,   274,   302,
    -152,    11,  -152,  -152
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,   107,
       4,     3,    11,    12,    13,    14,     5,     0,     0,     9,
       6,    10,     7,     8,    15,     0,     0,     0,     0,     0,
       0,     0,   174,    19,     0,     0,     0,   170,   171,     0,
     108,     0,     1,     2,     0,     0,     0,     0,    18,     0,
      25,    34,    80,     0,     0,     0,   102,   100,   103,   104,
     101,   175,    58,    56,    57,    59,    60,     0,     0,   111,
      94,   115,    84,    85,    88,    92,     0,    96,     0,   112,
      95,     0,   151,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    27,   175,    80,   160,     0,    16,     0,     0,
       0,    93,     0,   116,   113,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    30,     0,     0,     0,    32,
      24,     0,     0,     0,     0,     0,     0,    81,    61,    62,
      64,    66,     0,    95,     0,    28,     0,   172,     0,    98,
      99,   119,   118,   121,   122,   120,   117,    86,    87,    89,
      90,    91,     0,     0,   133,   123,   115,   150,    17,     0,
      45,    47,    50,    48,    49,    36,     0,    20,     0,     0,
      23,    35,     0,    26,    67,     0,     0,     0,     0,   158,
     157,   159,   154,   155,   156,     0,     0,     0,     0,     0,
       0,   161,   162,    97,     0,   106,   105,     0,   140,   137,
     138,   139,     0,    80,   134,     0,   135,     0,   124,   131,
     114,    31,     0,     0,     0,     0,    42,     0,    37,     0,
      33,     0,     0,    52,     0,    79,    63,    65,    68,     0,
       0,     0,    69,    71,     0,     0,     0,   173,   123,   123,
     145,   136,     0,   127,   126,   129,   130,   128,   125,     0,
       0,    39,    41,    38,    21,     0,    54,     0,     0,     0,
       0,     0,    70,     0,     0,    72,   144,   132,     0,    82,
     123,    46,     0,    44,     0,    43,    22,    53,     0,    73,
      77,    75,     0,     0,     0,     0,     0,     0,   164,   143,
       0,    40,    55,    74,    78,    76,   142,   152,   146,    83,
       0,   147,     0,    51,     0,     0,     0,   109,   141,   153,
     169,   163,   148,     0,    29,   168,   167,   165,     0,     0,
     110,   169,   149,   166
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -152,  -152,  -152,  -152,  -152,  -152,  -152,     0,  -152,   -83,
    -152,   224,  -152,  -151,  -152,  -124,  -123,  -152,   207,  -112,
    -152,   -86,  -152,   -34,  -152,   200,   -53,  -152,  -152,  -152,
    -152,  -152,  -152,  -152,   220,  -144,  -152,  -152,  -152,  -152,
     184,  -152,  -152,   -89,  -152,  -152,  -152,   252,  -152,  -152,
      66,  -152,  -152,    -3,   -43
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    17,    18,    19,    20,    21,    22,    99,   114,   118,
      90,   115,   165,   222,   173,    70,   127,   128,   129,   130,
     131,    92,   288,   132,    72,    73,    74,    75,    76,    77,
      41,   314,    78,    79,   104,   209,   154,   203,   204,   205,
     206,   269,   307,    80,   298,   185,    94,    95,   301,   311,
     317,    39,   138,    81,    82
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      23,    33,   133,   175,    36,   122,   123,    71,   135,    91,
      96,    24,   192,   174,    84,   101,    32,   274,    34,   315,
     275,   153,    44,    45,    46,   316,    48,    49,    50,   197,
      85,    51,    52,   100,   102,   197,   133,   133,     5,   207,
     116,     6,   119,    35,    87,   119,   119,     7,   223,     9,
     214,   215,   198,   216,   149,   150,   151,   199,   198,   200,
     231,    40,   229,   199,   137,   200,   227,    29,   201,    25,
     157,    43,   103,   134,   201,   232,    42,   208,   156,   261,
      54,   117,   217,   219,   202,    30,   120,    26,   133,   133,
     251,    96,   176,    47,   266,   267,    31,   218,    27,    28,
      53,    62,    63,    64,    65,    66,   223,   278,   155,    91,
     263,    61,    83,   284,    37,    38,   116,   240,   169,     5,
     105,   106,     6,   119,   152,   220,   289,   141,     7,   142,
       9,    86,   234,   277,   223,   279,   158,   159,   255,   235,
     223,   143,   144,   236,    55,   179,   180,   181,   145,    88,
     291,   228,    89,   146,    93,    56,   182,    57,   293,    97,
     237,  -174,   296,   111,   299,   121,   183,   184,    98,    58,
      59,   125,   107,   108,   109,   110,    60,   113,   119,   308,
     112,    61,    62,    63,    64,    65,    66,   124,   243,   126,
     244,   167,   168,    68,   238,   136,   133,   297,   133,   239,
     170,   168,   245,   246,   171,   168,   193,   194,     5,   247,
     139,     6,   140,   133,   248,   309,   310,     7,   166,     9,
     172,   179,   180,   181,   160,   161,   162,   254,   168,   321,
     260,   177,   182,    55,   140,   256,   257,   178,   186,   270,
     163,   164,   183,   184,    56,   187,    57,   188,   195,   189,
     196,     5,   212,   190,     6,   276,   168,   213,    58,    59,
       7,   221,     9,   225,   283,    60,   281,   257,   292,   257,
      61,    62,    63,    64,    65,    66,   295,   257,    67,     1,
     230,     2,    68,     3,     4,     5,   224,    55,     6,    62,
      63,    64,    65,    66,     7,     8,     9,   233,    56,   242,
      57,   249,   250,    10,    11,   147,   148,   253,   252,   258,
     259,   262,    58,    59,   268,    12,    13,    14,    15,    60,
     264,   265,    55,   271,    61,    62,    63,    64,    65,    66,
     273,   272,    67,    56,   280,    57,    68,    69,   282,   286,
      55,   285,   287,   290,   294,    61,    16,    58,    59,   125,
     300,    56,   302,    57,    60,   303,   305,   306,   313,    61,
      62,    63,    64,    65,    66,    58,    59,   126,   304,   312,
     320,    68,    60,   318,   319,   322,   210,    61,    62,    63,
      64,    65,    66,   211,   226,    67,   191,   323,   241,    68
};

static const yytype_int16 yycheck[] =
{
       0,     4,    91,   126,     7,    88,    89,    41,    94,    17,
      53,     4,   136,   125,    63,    68,    71,    29,    10,     8,
      32,   110,    25,    26,    27,    14,    29,    30,    31,    26,
      79,    34,    35,    67,    33,    26,   125,   126,     9,    33,
      83,    12,    85,    13,    47,    88,    89,    18,   172,    20,
      29,    30,    49,    32,   107,   108,   109,    54,    49,    56,
      61,    48,   186,    54,    98,    56,   178,     6,    65,     6,
     113,    77,    71,    81,    65,    76,     0,    71,   112,   230,
      78,    84,    61,   166,    81,    24,    86,    24,   177,   178,
     214,   134,   126,    24,   238,   239,    35,    76,    35,    36,
      19,    72,    73,    74,    75,    76,   230,   258,   111,    17,
     234,    71,    79,   264,    43,    44,   159,   203,   121,     9,
      82,    83,    12,   166,    84,   168,   270,    45,    18,    47,
      20,    33,    46,   257,   258,   259,    80,    81,   221,    53,
     264,    59,    60,    57,    34,    67,    68,    69,    66,    79,
     274,   185,    79,    71,    71,    45,    78,    47,   282,    75,
     194,    87,   285,    13,   287,    63,    88,    89,    79,    59,
      60,    61,    84,    85,    86,    79,    66,    87,   221,   302,
      81,    71,    72,    73,    74,    75,    76,    11,    45,    79,
      47,    80,    81,    83,   197,    78,   285,   286,   287,   202,
      80,    81,    59,    60,    80,    81,    80,    81,     9,    66,
      80,    12,    80,   302,    71,   304,   305,    18,    79,    20,
      79,    67,    68,    69,    21,    22,    23,    80,    81,   318,
     230,    64,    78,    34,    80,    80,    81,    25,    46,   242,
      37,    38,    88,    89,    45,    53,    47,    55,    80,    57,
      80,     9,    79,    61,    12,    80,    81,    79,    59,    60,
      18,    79,    20,    80,   264,    66,    80,    81,    80,    81,
      71,    72,    73,    74,    75,    76,    80,    81,    79,     3,
      79,     5,    83,     7,     8,     9,    81,    34,    12,    72,
      73,    74,    75,    76,    18,    19,    20,    72,    45,    26,
      47,    73,    73,    27,    28,   105,   106,    76,    31,    79,
      25,    76,    59,    60,    51,    39,    40,    41,    42,    66,
      79,    72,    34,    80,    71,    72,    73,    74,    75,    76,
      32,    81,    79,    45,    80,    47,    83,    84,    25,    16,
      34,    63,    52,    73,    80,    71,    70,    59,    60,    61,
      15,    45,    63,    47,    66,    80,    16,    58,    50,    71,
      72,    73,    74,    75,    76,    59,    60,    79,    81,    73,
      18,    83,    66,    81,    62,    73,   156,    71,    72,    73,
      74,    75,    76,   159,   177,    79,   134,   321,   204,    83
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,     5,     7,     8,     9,    12,    18,    19,    20,
      27,    28,    39,    40,    41,    42,    70,    91,    92,    93,
      94,    95,    96,    97,     4,     6,    24,    35,    36,     6,
      24,    35,    71,   143,    10,    13,   143,    43,    44,   141,
      48,   120,     0,    77,   143,   143,   143,    24,   143,   143,
     143,   143,   143,    19,    78,    34,    45,    47,    59,    60,
      66,    71,    72,    73,    74,    75,    76,    79,    83,    84,
     105,   113,   114,   115,   116,   117,   118,   119,   122,   123,
     133,   143,   144,    79,    63,    79,    33,   143,    79,    79,
     100,    17,   111,    71,   136,   137,   144,    75,    79,    97,
     113,   116,    33,    71,   124,    82,    83,    84,    85,    86,
      79,    13,    81,    87,    98,   101,   144,   143,    99,   144,
      97,    63,    99,    99,    11,    61,    79,   106,   107,   108,
     109,   110,   113,   133,    81,   111,    78,   113,   142,    80,
      80,    45,    47,    59,    60,    66,    71,   115,   115,   116,
     116,   116,    84,   133,   126,   143,   113,   144,    80,    81,
      21,    22,    23,    37,    38,   102,    79,    80,    81,   143,
      80,    80,    79,   104,   109,   106,   113,    64,    25,    67,
      68,    69,    78,    88,    89,   135,    46,    53,    55,    57,
      61,   137,   105,    80,    81,    80,    80,    26,    49,    54,
      56,    65,    81,   127,   128,   129,   130,    33,    71,   125,
     124,   101,    79,    79,    29,    30,    32,    61,    76,    99,
     144,    79,   103,   105,    81,    80,   108,   109,   113,   105,
      79,    61,    76,    72,    46,    53,    57,   113,   143,   143,
     111,   130,    26,    45,    47,    59,    60,    66,    71,    73,
      73,   105,    31,    76,    80,    99,    80,    81,    79,    25,
      97,   103,    76,   105,    79,    72,   125,   125,    51,   131,
     143,    80,    81,    32,    29,    32,    80,   105,   103,   105,
      80,    80,    25,    97,   103,    63,    16,    52,   112,   125,
      73,   105,    80,   105,    80,    80,   106,   133,   134,   106,
      15,   138,    63,    80,    81,    16,    58,   132,   106,   133,
     133,   139,    73,    50,   121,     8,    14,   140,    81,    62,
      18,   133,    73,   140
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,    90,    91,    91,    91,    91,    92,    92,    92,    92,
      92,    93,    93,    93,    93,    94,    95,    96,    96,    96,
      96,    96,    96,    96,    96,    96,    97,    97,    97,    97,
      98,    98,    99,    99,   100,   100,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   102,   102,   102,   102,   102,
     102,   102,   103,   103,   104,   104,   105,   105,   105,   105,
     105,   106,   107,   107,   108,   108,   109,   109,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     111,   111,   112,   112,   113,   114,   114,   114,   115,   115,
     115,   115,   116,   116,   117,   117,   117,   117,   117,   117,
     118,   118,   118,   118,   118,   119,   119,   120,   120,   121,
     121,   122,   122,   123,   123,   124,   124,   124,   124,   124,
     124,   124,   124,   125,   125,   125,   125,   125,   125,   125,
     125,   126,   126,   127,   127,   128,   128,   129,   129,   129,
     129,   130,   130,   130,   130,   131,   131,   132,   132,   132,
     133,   133,   134,   134,   135,   135,   135,   135,   135,   135,
     136,   136,   137,   138,   138,   139,   139,   140,   140,   140,
     141,   141,   142,   142,   143,   144
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     4,     6,     3,     2,
       6,     8,     9,     6,     5,     3,     6,     4,     5,    12,
       1,     3,     1,     3,     0,     3,     2,     3,     4,     4,
       6,     4,     3,     5,     5,     1,     4,     1,     1,     1,
       1,     6,     1,     3,     3,     5,     1,     1,     1,     1,
       1,     1,     1,     3,     1,     3,     1,     2,     3,     3,
       4,     3,     4,     5,     6,     5,     6,     5,     6,     3,
       0,     2,     0,     2,     1,     1,     3,     3,     1,     3,
       3,     3,     1,     2,     1,     1,     1,     4,     3,     3,
       1,     1,     1,     1,     1,     4,     4,     0,     1,     0,
       2,     1,     1,     2,     4,     0,     1,     2,     2,     2,
       2,     2,     2,     0,     1,     2,     2,     2,     2,     2,
       2,     2,     4,     0,     1,     1,     2,     1,     1,     1,
       1,     6,     5,     4,     3,     0,     3,     0,     2,     4,
       3,     1,     1,     3,     1,     1,     1,     1,     1,     1,
       1,     3,     3,     3,     0,     2,     4,     1,     1,     0,
       1,     1,     1,     3,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]));
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, &yylloc);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* start: stmt ';'  */
#line 80 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        parse_tree = (yyvsp[-1].sv_node);
        YYACCEPT;
    }
#line 1863 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 3: /* start: HELP  */
#line 85 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        parse_tree = std::make_shared<Help>();
        YYACCEPT;
    }
#line 1872 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 4: /* start: EXIT  */
#line 90 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
#line 1881 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 5: /* start: T_EOF  */
#line 95 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
#line 1890 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 11: /* txnStmt: TXN_BEGIN  */
#line 111 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnBegin>();
    }
#line 1898 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 12: /* txnStmt: TXN_COMMIT  */
#line 115 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnCommit>();
    }
#line 1906 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 13: /* txnStmt: TXN_ABORT  */
#line 119 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnAbort>();
    }
#line 1914 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 14: /* txnStmt: TXN_ROLLBACK  */
#line 123 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnRollback>();
    }
#line 1922 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 15: /* dbStmt: SHOW TABLES  */
#line 130 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<ShowTables>();
    }
#line 1930 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 16: /* setStmt: SET set_knob_type '=' VALUE_BOOL  */
#line 137 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<SetStmt>((yyvsp[-2].sv_setKnobType), (yyvsp[0].sv_bool));
    }
#line 1938 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 17: /* ddl: CREATE TABLE tbName '(' fieldList ')'  */
#line 144 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateTable>((yyvsp[-3].sv_str), (yyvsp[-1].sv_fields));
    }
#line 1946 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 18: /* ddl: DROP TABLE tbName  */
#line 148 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DropTable>((yyvsp[0].sv_str));
    }
#line 1954 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 19: /* ddl: DESC tbName  */
#line 152 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DescTable>((yyvsp[0].sv_str));
    }
#line 1962 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 20: /* ddl: CREATE INDEX tbName '(' colNameList ')'  */
#line 156 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateIndex>((yyvsp[-3].sv_str), (yyvsp[-1].sv_strs));
    }
#line 1970 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 21: /* ddl: CREATE INDEX tbName ON tbName '(' colNameList ')'  */
#line 160 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        // Alternative: CREATE INDEX name ON table (cols)
        (yyval.sv_node) = std::make_shared<CreateIndex>((yyvsp[-3].sv_str), (yyvsp[-1].sv_strs));
    }
#line 1979 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 22: /* ddl: CREATE UNIQUE INDEX tbName ON tbName '(' colNameList ')'  */
#line 165 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateIndex>((yyvsp[-3].sv_str), (yyvsp[-1].sv_strs));
    }
#line 1987 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 23: /* ddl: DROP INDEX tbName '(' colNameList ')'  */
#line 169 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DropIndex>((yyvsp[-3].sv_str), (yyvsp[-1].sv_strs));
    }
#line 1995 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 24: /* ddl: CREATE VIEW tbName AS dml  */
#line 173 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateView>((yyvsp[-2].sv_str), (yyvsp[0].sv_node));
    }
#line 2003 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 25: /* ddl: DROP VIEW tbName  */
#line 177 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DropView>((yyvsp[0].sv_str));
    }
#line 2011 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 26: /* dml: INSERT INTO tbName optColList VALUES valueTupleList  */
#line 184 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<InsertStmt>((yyvsp[-3].sv_str), (yyvsp[-2].sv_strs), (yyvsp[0].sv_vals_list));
    }
#line 2019 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 27: /* dml: DELETE FROM tbName optWhereClause  */
#line 188 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DeleteStmt>((yyvsp[-1].sv_str), (yyvsp[0].sv_cond));
    }
#line 2027 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 28: /* dml: UPDATE tbName SET setClauses optWhereClause  */
#line 192 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_node) = std::make_shared<UpdateStmt>((yyvsp[-3].sv_str), (yyvsp[-1].sv_set_clauses), (yyvsp[0].sv_cond));
    }
#line 2035 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 29: /* dml: SELECT optDistinct selectItems FROM fromList optJoinList optWhereClause optGroupBy optHaving opt_order_clause optLimit optForUpdateClause  */
#line 197 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        auto stmt = std::make_shared<SelectStmt>();
        stmt->has_distinct = (yyvsp[-10].sv_bool);
        // separate columns, aggregates, and expressions from select items
        for (auto &e : (yyvsp[-9].sv_exprs)) {
            if (auto c = std::dynamic_pointer_cast<Col>(e)) {
                stmt->cols.push_back(c);
            } else if (auto a = std::dynamic_pointer_cast<AggExpr>(e)) {
                stmt->aggs.push_back(a);
                stmt->is_agg = true;
            } else {
                stmt->exprs.push_back(e);
            }
        }
        stmt->tabs = (yyvsp[-7].sv_strs);
        stmt->joins = (yyvsp[-6].sv_joins);
        // Collect join table names and aliases (alias AFTER table, like fromList)
        for (auto &j : stmt->joins) {
            stmt->tabs.push_back(j->tab_name);
            if (!j->alias.empty()) {
                stmt->table_aliases[j->alias] = j->tab_name;
                stmt->tabs.push_back(j->alias);
            }
        }
        stmt->cond = (yyvsp[-5].sv_cond);
        stmt->group_by = (yyvsp[-4].sv_groupby);
        stmt->having = (yyvsp[-3].sv_cond);
        stmt->order = (yyvsp[-2].sv_orderby);
        stmt->limit = (yyvsp[-1].sv_limit);
        stmt->is_for_update = (yyvsp[0].sv_bool);
        (yyval.sv_node) = stmt;
    }
#line 2072 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 30: /* fieldList: field  */
#line 233 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_fields) = std::vector<std::shared_ptr<Field>>{(yyvsp[0].sv_field)};
    }
#line 2080 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 31: /* fieldList: fieldList ',' field  */
#line 237 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_fields).push_back((yyvsp[0].sv_field));
    }
#line 2088 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 32: /* colNameList: colName  */
#line 244 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_strs) = std::vector<std::string>{(yyvsp[0].sv_str)};
    }
#line 2096 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 33: /* colNameList: colNameList ',' colName  */
#line 248 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[0].sv_str));
    }
#line 2104 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 34: /* optColList: %empty  */
#line 255 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_strs) = std::vector<std::string>{};
    }
#line 2112 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 35: /* optColList: '(' colNameList ')'  */
#line 259 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_strs) = (yyvsp[-1].sv_strs);
    }
#line 2120 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 36: /* field: colName type  */
#line 266 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_field) = std::make_shared<ColDef>((yyvsp[-1].sv_str), (yyvsp[0].sv_type_len), false, nullptr);
    }
#line 2128 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 37: /* field: colName type VALUE_NULL  */
#line 270 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_field) = std::make_shared<ColDef>((yyvsp[-2].sv_str), (yyvsp[-1].sv_type_len), false, nullptr);
    }
#line 2136 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 38: /* field: colName type NOT VALUE_NULL  */
#line 274 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_field) = std::make_shared<ColDef>((yyvsp[-3].sv_str), (yyvsp[-2].sv_type_len), true, nullptr);
    }
#line 2144 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 39: /* field: colName type DEFAULT value  */
#line 278 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_field) = std::make_shared<ColDef>((yyvsp[-3].sv_str), (yyvsp[-2].sv_type_len), false, (yyvsp[0].sv_val));
    }
#line 2152 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 40: /* field: colName type NOT VALUE_NULL DEFAULT value  */
#line 282 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_field) = std::make_shared<ColDef>((yyvsp[-5].sv_str), (yyvsp[-4].sv_type_len), true, (yyvsp[0].sv_val));
    }
#line 2160 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 41: /* field: colName type PRIMARY KEY  */
#line 286 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        auto col = std::make_shared<ColDef>((yyvsp[-3].sv_str), (yyvsp[-2].sv_type_len), true, nullptr);
        col->primary_key_ = true;
        (yyval.sv_field) = col;
    }
#line 2170 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 42: /* field: colName type AUTO_INCREMENT  */
#line 292 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        auto col = std::make_shared<ColDef>((yyvsp[-2].sv_str), (yyvsp[-1].sv_type_len), false, nullptr);
        col->auto_increment_ = true;
        (yyval.sv_field) = col;
    }
#line 2180 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 43: /* field: colName type NOT VALUE_NULL AUTO_INCREMENT  */
#line 298 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        auto col = std::make_shared<ColDef>((yyvsp[-4].sv_str), (yyvsp[-3].sv_type_len), true, nullptr);
        col->auto_increment_ = true;
        (yyval.sv_field) = col;
    }
#line 2190 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 44: /* field: colName type PRIMARY KEY AUTO_INCREMENT  */
#line 304 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        auto col = std::make_shared<ColDef>((yyvsp[-4].sv_str), (yyvsp[-3].sv_type_len), true, nullptr);
        col->primary_key_ = true;
        col->auto_increment_ = true;
        (yyval.sv_field) = col;
    }
#line 2201 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 45: /* type: INT  */
#line 314 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_INT, sizeof(int));
    }
#line 2209 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 46: /* type: CHAR '(' VALUE_INT ')'  */
#line 318 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        // Multiply by 4 for UTF-8 multi-byte character support
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_STRING, (yyvsp[-1].sv_int) * 4);
    }
#line 2218 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 47: /* type: CHAR  */
#line 323 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        // CHAR without length defaults to 4 bytes (1 char)
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_STRING, 4);
    }
#line 2227 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 48: /* type: DATE  */
#line 328 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_STRING, 256);
    }
#line 2235 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 49: /* type: TEXT  */
#line 332 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_STRING, 256);
    }
#line 2243 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 50: /* type: FLOAT  */
#line 336 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_FLOAT, sizeof(float));
    }
#line 2251 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 51: /* type: FLOAT '(' VALUE_INT ',' VALUE_INT ')'  */
#line 340 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        // DECIMAL(p,s) / NUMERIC(p,s): ignore precision/scale, store as FLOAT
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_FLOAT, sizeof(float));
    }
#line 2260 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 52: /* valueList: value  */
#line 348 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_vals) = std::vector<std::shared_ptr<Value>>{(yyvsp[0].sv_val)};
    }
#line 2268 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 53: /* valueList: valueList ',' value  */
#line 352 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_vals).push_back((yyvsp[0].sv_val));
    }
#line 2276 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 54: /* valueTupleList: '(' valueList ')'  */
#line 359 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_vals_list) = std::vector<std::vector<std::shared_ptr<Value>>>{(yyvsp[-1].sv_vals)};
    }
#line 2284 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 55: /* valueTupleList: valueTupleList ',' '(' valueList ')'  */
#line 363 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_vals_list).push_back((yyvsp[-1].sv_vals));
    }
#line 2292 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 56: /* value: VALUE_INT  */
#line 370 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_val) = std::make_shared<IntLit>((yyvsp[0].sv_int));
    }
#line 2300 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 57: /* value: VALUE_FLOAT  */
#line 374 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_val) = std::make_shared<FloatLit>((yyvsp[0].sv_float));
    }
#line 2308 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 58: /* value: VALUE_STRING  */
#line 378 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_val) = std::make_shared<StringLit>((yyvsp[0].sv_str));
    }
#line 2316 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 59: /* value: VALUE_BOOL  */
#line 382 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_val) = std::make_shared<BoolLit>((yyvsp[0].sv_bool));
    }
#line 2324 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 60: /* value: VALUE_NULL  */
#line 386 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_val) = std::make_shared<NullLit>();
    }
#line 2332 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 63: /* cond_or: cond_or OR cond_and  */
#line 403 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<LogicExpr>(LOGIC_OR,
              std::vector<std::shared_ptr<CondExpr>>{(yyvsp[-2].sv_cond), (yyvsp[0].sv_cond)});
    }
#line 2341 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 65: /* cond_and: cond_and AND cond_not  */
#line 412 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<LogicExpr>(LOGIC_AND,
              std::vector<std::shared_ptr<CondExpr>>{(yyvsp[-2].sv_cond), (yyvsp[0].sv_cond)});
    }
#line 2350 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 67: /* cond_not: NOT cond_not  */
#line 421 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<LogicExpr>(LOGIC_NOT,
              std::vector<std::shared_ptr<CondExpr>>{(yyvsp[0].sv_cond)});
    }
#line 2359 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 68: /* cond_base: expr op expr  */
#line 429 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<BinaryExpr>((yyvsp[-2].sv_expr), (yyvsp[-1].sv_comp_op), (yyvsp[0].sv_expr));
    }
#line 2367 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 69: /* cond_base: col IS VALUE_NULL  */
#line 433 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<UnaryCondExpr>((yyvsp[-2].sv_col), SV_OP_IS_NULL);
    }
#line 2375 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 70: /* cond_base: col IS NOT VALUE_NULL  */
#line 437 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<UnaryCondExpr>((yyvsp[-3].sv_col), SV_OP_IS_NOT_NULL);
    }
#line 2383 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 71: /* cond_base: col LIKE VALUE_STRING  */
#line 441 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<LikeExpr>((yyvsp[-2].sv_col), false, (yyvsp[0].sv_str));
    }
#line 2391 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 72: /* cond_base: col NOT LIKE VALUE_STRING  */
#line 445 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<LikeExpr>((yyvsp[-3].sv_col), true, (yyvsp[0].sv_str));
    }
#line 2399 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 73: /* cond_base: col BETWEEN value AND value  */
#line 449 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<BetweenExpr>((yyvsp[-4].sv_col), false, (yyvsp[-2].sv_val), (yyvsp[0].sv_val));
    }
#line 2407 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 74: /* cond_base: col NOT BETWEEN value AND value  */
#line 453 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<BetweenExpr>((yyvsp[-5].sv_col), true, (yyvsp[-2].sv_val), (yyvsp[0].sv_val));
    }
#line 2415 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 75: /* cond_base: col IN '(' valueList ')'  */
#line 457 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<InExpr>((yyvsp[-4].sv_col), false, (yyvsp[-1].sv_vals));
    }
#line 2423 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 76: /* cond_base: col NOT IN '(' valueList ')'  */
#line 461 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<InExpr>((yyvsp[-5].sv_col), true, (yyvsp[-1].sv_vals));
    }
#line 2431 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 77: /* cond_base: col IN '(' dml ')'  */
#line 465 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<InExpr>((yyvsp[-4].sv_col), false, std::vector<std::shared_ptr<Value>>{}, (yyvsp[-1].sv_node));
    }
#line 2439 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 78: /* cond_base: col NOT IN '(' dml ')'  */
#line 469 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<InExpr>((yyvsp[-5].sv_col), true, std::vector<std::shared_ptr<Value>>{}, (yyvsp[-1].sv_node));
    }
#line 2447 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 79: /* cond_base: '(' condition ')'  */
#line 473 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = (yyvsp[-1].sv_cond);
    }
#line 2455 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 80: /* optWhereClause: %empty  */
#line 480 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = nullptr;
    }
#line 2463 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 81: /* optWhereClause: WHERE condition  */
#line 484 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = (yyvsp[0].sv_cond);
    }
#line 2471 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 82: /* optHaving: %empty  */
#line 491 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = nullptr;
    }
#line 2479 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 83: /* optHaving: HAVING condition  */
#line 495 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cond) = (yyvsp[0].sv_cond);
    }
#line 2487 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 86: /* expr_add_sub: expr_add_sub '+' expr_mul_div  */
#line 511 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ArithExpr>((yyvsp[-2].sv_expr), ARITH_ADD, (yyvsp[0].sv_expr));
    }
#line 2495 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 87: /* expr_add_sub: expr_add_sub '-' expr_mul_div  */
#line 515 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ArithExpr>((yyvsp[-2].sv_expr), ARITH_SUB, (yyvsp[0].sv_expr));
    }
#line 2503 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 89: /* expr_mul_div: expr_mul_div '*' expr_unary  */
#line 523 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ArithExpr>((yyvsp[-2].sv_expr), ARITH_MUL, (yyvsp[0].sv_expr));
    }
#line 2511 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 90: /* expr_mul_div: expr_mul_div '/' expr_unary  */
#line 527 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ArithExpr>((yyvsp[-2].sv_expr), ARITH_DIV, (yyvsp[0].sv_expr));
    }
#line 2519 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 91: /* expr_mul_div: expr_mul_div '%' expr_unary  */
#line 531 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ArithExpr>((yyvsp[-2].sv_expr), ARITH_MOD, (yyvsp[0].sv_expr));
    }
#line 2527 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 93: /* expr_unary: '-' expr_unary  */
#line 539 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ArithExpr>(nullptr, ARITH_NEG, (yyvsp[0].sv_expr));
    }
#line 2535 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 94: /* expr_base: value  */
#line 546 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::static_pointer_cast<Expr>((yyvsp[0].sv_val));
    }
#line 2543 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 95: /* expr_base: col  */
#line 550 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::static_pointer_cast<Expr>((yyvsp[0].sv_col));
    }
#line 2551 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 96: /* expr_base: aggExpr  */
#line 554 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::static_pointer_cast<Expr>((yyvsp[0].sv_agg));
    }
#line 2559 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 97: /* expr_base: CONCAT '(' expr_list ')'  */
#line 558 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ConcatExpr>((yyvsp[-1].sv_exprs));
    }
#line 2567 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 98: /* expr_base: '(' dml ')'  */
#line 562 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<SubqueryExpr>((yyvsp[-1].sv_node));
    }
#line 2575 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 99: /* expr_base: '(' expr ')'  */
#line 566 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_expr) = (yyvsp[-1].sv_expr);
    }
#line 2583 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 100: /* aggType: COUNT  */
#line 576 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_agg_type) = AGG_COUNT; }
#line 2589 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 101: /* aggType: SUM  */
#line 577 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_agg_type) = AGG_SUM; }
#line 2595 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 102: /* aggType: AVG  */
#line 578 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_agg_type) = AGG_AVG; }
#line 2601 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 103: /* aggType: MAX  */
#line 579 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_agg_type) = AGG_MAX; }
#line 2607 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 104: /* aggType: MIN  */
#line 580 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_agg_type) = AGG_MIN; }
#line 2613 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 105: /* aggExpr: aggType '(' col ')'  */
#line 585 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_agg) = std::make_shared<AggExpr>((yyvsp[-3].sv_agg_type), (yyvsp[-1].sv_col)->col_name);
    }
#line 2621 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 106: /* aggExpr: aggType '(' '*' ')'  */
#line 589 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        if ((yyvsp[-3].sv_agg_type) == AGG_COUNT) {
            (yyval.sv_agg) = std::make_shared<AggExpr>(AGG_COUNT, "", true);
        } else {
            yyerror(&yylloc, "Only COUNT supports * argument");
            YYERROR;
        }
    }
#line 2634 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 107: /* optDistinct: %empty  */
#line 604 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_bool) = false; }
#line 2640 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 108: /* optDistinct: DISTINCT  */
#line 605 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_bool) = true; }
#line 2646 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 109: /* optForUpdateClause: %empty  */
#line 609 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_bool) = false; }
#line 2652 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 110: /* optForUpdateClause: FOR UPDATE  */
#line 610 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_bool) = true; }
#line 2658 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 111: /* selectItems: '*'  */
#line 615 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_exprs) = std::vector<std::shared_ptr<Expr>>{};
    }
#line 2666 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 113: /* selectItemList: expr opt_alias  */
#line 623 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_exprs) = std::vector<std::shared_ptr<Expr>>{(yyvsp[-1].sv_expr)};
    }
#line 2674 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 114: /* selectItemList: selectItemList ',' expr opt_alias  */
#line 627 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_exprs).push_back((yyvsp[-1].sv_expr));
    }
#line 2682 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 115: /* opt_alias: %empty  */
#line 633 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { }
#line 2688 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 116: /* opt_alias: IDENTIFIER  */
#line 634 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { }
#line 2694 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 117: /* opt_alias: AS IDENTIFIER  */
#line 635 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { }
#line 2700 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 118: /* opt_alias: AS COUNT  */
#line 636 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { }
#line 2706 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 119: /* opt_alias: AS AVG  */
#line 637 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { }
#line 2712 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 120: /* opt_alias: AS SUM  */
#line 638 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { }
#line 2718 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 121: /* opt_alias: AS MAX  */
#line 639 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { }
#line 2724 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 122: /* opt_alias: AS MIN  */
#line 640 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { }
#line 2730 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 123: /* optAliasStr: %empty  */
#line 644 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_str) = ""; }
#line 2736 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 124: /* optAliasStr: IDENTIFIER  */
#line 645 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_str) = (yyvsp[0].sv_str); }
#line 2742 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 125: /* optAliasStr: AS IDENTIFIER  */
#line 646 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_str) = (yyvsp[0].sv_str); }
#line 2748 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 126: /* optAliasStr: AS COUNT  */
#line 647 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_str) = "count"; }
#line 2754 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 127: /* optAliasStr: AS AVG  */
#line 648 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_str) = "avg"; }
#line 2760 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 128: /* optAliasStr: AS SUM  */
#line 649 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_str) = "sum"; }
#line 2766 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 129: /* optAliasStr: AS MAX  */
#line 650 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_str) = "max"; }
#line 2772 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 130: /* optAliasStr: AS MIN  */
#line 651 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                        { (yyval.sv_str) = "min"; }
#line 2778 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 131: /* fromList: tbName optAliasStr  */
#line 656 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_strs) = std::vector<std::string>{(yyvsp[-1].sv_str)};
        if (!(yyvsp[0].sv_str).empty()) (yyval.sv_strs).push_back((yyvsp[0].sv_str));
    }
#line 2787 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 132: /* fromList: fromList ',' tbName optAliasStr  */
#line 661 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[-1].sv_str));
        if (!(yyvsp[0].sv_str).empty()) (yyval.sv_strs).push_back((yyvsp[0].sv_str));
    }
#line 2796 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 133: /* optJoinList: %empty  */
#line 669 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_joins) = std::vector<std::shared_ptr<JoinExpr>>{};
    }
#line 2804 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 134: /* optJoinList: joinList  */
#line 673 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_joins) = (yyvsp[0].sv_joins);
    }
#line 2812 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 135: /* joinList: joinClause  */
#line 680 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_joins) = std::vector<std::shared_ptr<JoinExpr>>{(yyvsp[0].sv_join)};
    }
#line 2820 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 136: /* joinList: joinList joinClause  */
#line 684 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_joins).push_back((yyvsp[0].sv_join));
    }
#line 2828 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 137: /* joinType: INNER  */
#line 690 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_int) = INNER_JOIN; }
#line 2834 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 138: /* joinType: LEFT  */
#line 691 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_int) = LEFT_JOIN; }
#line 2840 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 139: /* joinType: RIGHT  */
#line 692 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_int) = RIGHT_JOIN; }
#line 2846 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 140: /* joinType: FULL  */
#line 693 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_int) = FULL_JOIN; }
#line 2852 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 141: /* joinClause: joinType JOIN tbName optAliasStr ON condition  */
#line 698 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_join) = std::make_shared<JoinExpr>((yyvsp[-3].sv_str), (yyvsp[0].sv_cond), static_cast<JoinType>((yyvsp[-5].sv_int)), (yyvsp[-2].sv_str));
    }
#line 2860 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 142: /* joinClause: JOIN tbName optAliasStr ON condition  */
#line 702 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_join) = std::make_shared<JoinExpr>((yyvsp[-3].sv_str), (yyvsp[0].sv_cond), INNER_JOIN, (yyvsp[-2].sv_str));
    }
#line 2868 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 143: /* joinClause: joinType JOIN tbName optAliasStr  */
#line 706 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_join) = std::make_shared<JoinExpr>((yyvsp[-1].sv_str), nullptr, static_cast<JoinType>((yyvsp[-3].sv_int)), (yyvsp[0].sv_str));
    }
#line 2876 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 144: /* joinClause: JOIN tbName optAliasStr  */
#line 710 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_join) = std::make_shared<JoinExpr>((yyvsp[-1].sv_str), nullptr, INNER_JOIN, (yyvsp[0].sv_str));
    }
#line 2884 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 145: /* optGroupBy: %empty  */
#line 717 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_groupby) = nullptr;
    }
#line 2892 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 146: /* optGroupBy: GROUP BY colList  */
#line 721 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_groupby) = std::make_shared<GroupBy>((yyvsp[0].sv_cols));
    }
#line 2900 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 147: /* optLimit: %empty  */
#line 728 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_limit) = nullptr;
    }
#line 2908 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 148: /* optLimit: LIMIT VALUE_INT  */
#line 732 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_limit) = std::make_shared<LimitClause>((yyvsp[0].sv_int));
    }
#line 2916 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 149: /* optLimit: LIMIT VALUE_INT OFFSET VALUE_INT  */
#line 736 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_limit) = std::make_shared<LimitClause>((yyvsp[-2].sv_int), (yyvsp[0].sv_int));
    }
#line 2924 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 150: /* col: tbName '.' colName  */
#line 743 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_col) = std::make_shared<Col>((yyvsp[-2].sv_str), (yyvsp[0].sv_str));
    }
#line 2932 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 151: /* col: colName  */
#line 747 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_col) = std::make_shared<Col>("", (yyvsp[0].sv_str));
    }
#line 2940 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 152: /* colList: col  */
#line 754 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cols) = std::vector<std::shared_ptr<Col>>{(yyvsp[0].sv_col)};
    }
#line 2948 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 153: /* colList: colList ',' col  */
#line 758 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_cols).push_back((yyvsp[0].sv_col));
    }
#line 2956 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 154: /* op: '='  */
#line 765 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_EQ;
    }
#line 2964 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 155: /* op: '<'  */
#line 769 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_LT;
    }
#line 2972 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 156: /* op: '>'  */
#line 773 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_GT;
    }
#line 2980 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 157: /* op: NEQ  */
#line 777 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_NE;
    }
#line 2988 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 158: /* op: LEQ  */
#line 781 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_LE;
    }
#line 2996 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 159: /* op: GEQ  */
#line 785 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_GE;
    }
#line 3004 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 160: /* setClauses: setClause  */
#line 792 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_set_clauses) = std::vector<std::shared_ptr<SetClause>>{(yyvsp[0].sv_set_clause)};
    }
#line 3012 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 161: /* setClauses: setClauses ',' setClause  */
#line 796 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_set_clauses).push_back((yyvsp[0].sv_set_clause));
    }
#line 3020 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 162: /* setClause: colName '=' value  */
#line 803 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_set_clause) = std::make_shared<SetClause>((yyvsp[-2].sv_str), (yyvsp[0].sv_val));
    }
#line 3028 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 163: /* opt_order_clause: ORDER BY order_clause  */
#line 810 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_orderby) = (yyvsp[0].sv_orderby);
    }
#line 3036 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 164: /* opt_order_clause: %empty  */
#line 813 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                    { /* ignore*/ }
#line 3042 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 165: /* order_clause: col opt_asc_desc  */
#line 818 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_orderby) = std::make_shared<OrderBy>((yyvsp[-1].sv_col), (yyvsp[0].sv_orderby_dir));
    }
#line 3050 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 166: /* order_clause: order_clause ',' col opt_asc_desc  */
#line 822 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        // Multi-column ORDER BY — only first column is used
    }
#line 3058 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 167: /* opt_asc_desc: ASC  */
#line 828 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_orderby_dir) = OrderBy_ASC; }
#line 3064 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 168: /* opt_asc_desc: DESC  */
#line 829 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_orderby_dir) = OrderBy_DESC; }
#line 3070 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 169: /* opt_asc_desc: %empty  */
#line 830 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                { (yyval.sv_orderby_dir) = OrderBy_DEFAULT; }
#line 3076 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 170: /* set_knob_type: ENABLE_NESTLOOP  */
#line 834 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                         { (yyval.sv_setKnobType) = EnableNestLoop; }
#line 3082 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 171: /* set_knob_type: ENABLE_SORTMERGE  */
#line 835 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
                         { (yyval.sv_setKnobType) = EnableSortMerge; }
#line 3088 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 172: /* expr_list: expr  */
#line 840 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_exprs) = std::vector<std::shared_ptr<Expr>>{(yyvsp[0].sv_expr)};
    }
#line 3096 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;

  case 173: /* expr_list: expr_list ',' expr  */
#line 844 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"
    {
        (yyval.sv_exprs).push_back((yyvsp[0].sv_expr));
    }
#line 3104 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"
    break;


#line 3108 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.tab.cpp"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (&yylloc, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

#line 852 "/home/obuser/Rubase/db2025/rmdb/src/parser/yacc.y"

