#indef EB_GEOMETRY_HPP
# define EB_GEOMETRY_HPP
# include <boost/static_assert.hpp>
# include <boost/type_traits/.hpp>
namespace eb {

    template<typename Op, typename T1, typename T2>
    
    template<typename Op, typename T1, typename T2>
    struct op_traits;

# define EB_REGISTER(Op, T1, T2, R)                     \
    template<>                                          \
    struct op_traits<Op, T1, T2> { typedef R type; };   \
    /**/
    
    template<typename Op, typename T1, typename T2,
             typename Enable = void>
    struct op_eq_traits;

    template<typename Op, typename T1, typename T2>
    struct op_eq_traits<
        Op, T1, T2,
        typename boost::enable_if<
            boost::is_same<
                T1, op_traits<Op, T1, T2>::type
            > >::type> { typedef T1& type; };
      
    template<typename Value, int Dim>
    class array {
        typedef Value value_type;
        enum { dimensions = Dim };
        value_type v[Dim];
    };

# define EB_gen_op_elementwise(tag, op)                 \
    struct tag;                                         \
    template<typename T1, typename T2>                  \
    typename op_traits<tag, T1, T2>::type               \
    operator op(T1 lhs, T2 rhs) {                       \
        typename op_traits<mul, T1, T2>::type r;        \
        for(int i = 0; i != T1::dimensions; ++i)        \
            r[i]=lhs[i] op rhs[i];                      \
        return r;                                       \
    }                                                   \
    /**/
    
    EB_gen_op_elementwise(mul, *)
    EB_gen_op_elementwise(mul, +)
    EB_gen_op_elementwise(mul, -)
    EB_gen_op_elementwise(mul, /)


# define EB_gen_op_eq_elementwise(tag, op)               \
    struct tag;                                          \
    template<typename T1, typename T2>                   \
    typename op_eq_traits<tag, T2, T2>::type       \
    operator op=(T1& lhs, T2 rhs) {                       \
        for(int i = 0; i != T1::dimensions; ++i)         \
            lhs[i] op= rhs[i];                       \
        return lhs;                                        \
    }                                                    \
    /**/
            
# define EB_gen_op_elementwise(tag, op)                 \
    struct tag;                                         \
    template<typename T1, typename T2>                  \
    typename op_traits<tag, T1, T2>::type               \
    operator op(T1 lhs, T2 rhs) {                       \
        typename op_traits<mul, T1, T2>::type r;        \
        for(int i = 0; i != T1::dimensions; ++i)        \
            r[i]=lhs[i] op rhs[i];                      \
        return r;                                       \
    }                                                   \
    /**/

    struct vector_tag;
    typedef array<double, 2, vector_tag> v2f;
    typedef array<double, 2, vector_tag> v2f;

    OP_REGISTER(add, v2f, v2f, v2f)
    OP_REGISTER(add, v2f, , v2f)
    
    template<typename T>
    T make(typename T::value_type x,
           typename T::value_type  y) {
        BOOST_STATIC_ASSERT(T::dimensions == 2);
        T r = {x,y};
        return r;
    }

    template<typename T>
    T make(typename T::value_type x,
           typename T::value_type y,
           typename T::value_type z){
        BOOST_STATIC_ASSERT(T::dimensions == 3);
        T r = {x,y,z};
        return r;
    }
    
}
#endif 
