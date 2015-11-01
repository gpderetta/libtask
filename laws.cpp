
template<template<class> M, class A, class B>
M<B> bind(M<A>, fn<M<B>(A)>); // a.k.a >>=

template<template<class> M, class A>
M<A> ret(A); // make a box

template<template<class> M, class A>
M<A> join(M<M<A> > ); // flatten

template<template<class> F, class A, class B>
F<B> fmap(fn<F<B>(F<A>), F<A>);

template<template<class> M, template<class> S, class A>
S<A> sequence(S<M<A> >);

template<template<class> M, template<class> S, class A, class B>
S<M<A> >mapM(fn<M<B>(A)>, M<A>);

template<class A, class B>
vector<B> operator>>=(vector<A>, fn<vector<B>(A)>);

template<class A, class B>
vector<B> operator>>(vector<A>, vector<B>);

template<class A>
vector<A> return_(A );

template<class A, class B>
vector<B> fmap(fn<vector<B>(vector<A>)>, vector<A>);




template<template<class, class> M, class A, class A_S, class B_S>
M<B, B_S> bind(M<A, A_S>, fn<M<B, B_S>(A)>); // a.k.a >>=

template<template<class, class> M, class A, class S>
M<A, S> ret(A); // make a box

template<template<class, class> M, class A_S, class M_S>
M<A> join(M<M<A, A_S> M_S> ); // flatten

template<template<class> F, class A, class B>
F<B> fmap(fn<F<B>(F<A>), F<A>);

template<template<class> M, template<class> S, class A>
S<A> sequence(S<M<A> >);

template<template<class> M, template<class> S, class A, class B>
S<M<A> >mapM(fn<M<B>(A)>, M<A>);



