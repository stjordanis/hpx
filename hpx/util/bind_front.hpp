//  Copyright (c) 2011-2012 Thomas Heller
//  Copyright (c) 2013-2017 Agustin Berge
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_UTIL_BIND_FRONT_HPP
#define HPX_UTIL_BIND_FRONT_HPP

#include <hpx/config.hpp>
#include <hpx/traits/get_function_address.hpp>
#include <hpx/traits/get_function_annotation.hpp>
#include <hpx/util/decay.hpp>
#include <hpx/util/detail/pack.hpp>
#include <hpx/util/invoke.hpp>
#include <hpx/util/result_of.hpp>
#include <hpx/util/tuple.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace hpx { namespace util
{
    namespace detail
    {
        template <typename F>
        class one_shot_wrapper;

        ///////////////////////////////////////////////////////////////////////
        template <typename F, typename Ts, typename ...Us>
        struct invoke_bound_front_result;

        template <typename F, typename ...Ts, typename ...Us>
        struct invoke_bound_front_result<F&, util::tuple<Ts...>&, Us...>
          : util::invoke_result<F&, Ts&..., Us...>
        {};

        template <typename F, typename ...Ts, typename ...Us>
        struct invoke_bound_front_result<F const&, util::tuple<Ts...> const&, Us...>
          : util::invoke_result<F const&, Ts const&..., Us...>
        {};

        template <typename F, typename ...Ts, typename ...Us>
        struct invoke_bound_front_result<F&&, util::tuple<Ts...>&&, Us...>
          : util::invoke_result<F, Ts..., Us...>
        {};

        template <typename F, typename ...Ts, typename ...Us>
        struct invoke_bound_front_result<F const&&, util::tuple<Ts...> const&&, Us...>
          : util::invoke_result<F const, Ts const..., Us...>
        {};

        // one-shot wrapper is not const callable
        template <typename F, typename ...Ts, typename ...Us>
        struct invoke_bound_front_result<
            one_shot_wrapper<F> const&, util::tuple<Ts...> const&, Us...>
        {};

        template <typename F, typename ...Ts, typename ...Us>
        struct invoke_bound_front_result<
            one_shot_wrapper<F> const&&, util::tuple<Ts...> const&&, Us...>
        {};

        template <typename F, std::size_t ...Is, typename Ts, typename ...Us>
        HPX_CONSTEXPR HPX_HOST_DEVICE
        typename invoke_bound_front_result<F&&, Ts&&, Us...>::type
        bound_front_impl(F&& f, pack_c<std::size_t, Is...>, Ts&& bound,
            Us&&... unbound)
        {
            return util::invoke(std::forward<F>(f),
                util::get<Is>(std::forward<Ts>(bound))...,
                std::forward<Us>(unbound)...);
        }

        ///////////////////////////////////////////////////////////////////////
        template <typename F, typename ...Ts>
        struct bound_front
        {
        public:
            bound_front() {} // needed for serialization

            template <typename F_, typename ...Ts_, typename =
                typename std::enable_if<
                    !std::is_same<typename std::decay<F_>::type, bound_front>::value
                >::type>
            HPX_CONSTEXPR explicit bound_front(F_&& f, Ts_&&... vs)
              : _f(std::forward<F_>(f))
              , _args(std::forward<Ts_>(vs)...)
            {}

#if !defined(__NVCC__) && !defined(__CUDACC__)
            bound_front(bound_front const&) = default;
            bound_front(bound_front&&) = default;
#else
            HPX_CONSTEXPR HPX_HOST_DEVICE bound_front(bound_front const& other)
              : _f(other._f)
              , _args(other._args)
            {}

            HPX_CONSTEXPR HPX_HOST_DEVICE bound_front(bound_front&& other)
              : _f(std::move(other._f))
              , _args(std::move(other._args))
            {}
#endif

            bound_front& operator=(bound_front const&) = delete;

            template <typename ...Us>
            HPX_CXX14_CONSTEXPR HPX_HOST_DEVICE
            typename invoke_bound_front_result<
                typename std::decay<F>::type&,
                util::tuple<typename util::decay_unwrap<Ts>::type...>&,
                Us...
            >::type operator()(Us&&... vs) &
            {
                return detail::bound_front_impl(_f,
                    typename detail::make_index_pack<sizeof...(Ts)>::type(),
                    _args, std::forward<Us>(vs)...);
            }

            template <typename ...Us>
            HPX_CONSTEXPR HPX_HOST_DEVICE
            typename invoke_bound_front_result<
                typename std::decay<F>::type const&,
                util::tuple<typename util::decay_unwrap<Ts>::type...> const&,
                Us...
            >::type operator()(Us&&... vs) const&
            {
                return detail::bound_front_impl(_f,
                    typename detail::make_index_pack<sizeof...(Ts)>::type(),
                    _args, std::forward<Us>(vs)...);
            }

            template <typename ...Us>
            HPX_CXX14_CONSTEXPR HPX_HOST_DEVICE
            typename invoke_bound_front_result<
                typename std::decay<F>::type&&,
                util::tuple<typename util::decay_unwrap<Ts>::type...>&&,
                Us...
            >::type operator()(Us&&... vs) &&
            {
                return detail::bound_front_impl(std::move(_f),
                    typename detail::make_index_pack<sizeof...(Ts)>::type(),
                    std::move(_args), std::forward<Us>(vs)...);
            }

            template <typename ...Us>
            HPX_CONSTEXPR HPX_HOST_DEVICE
            typename invoke_bound_front_result<
                typename std::decay<F>::type const&&,
                util::tuple<typename util::decay_unwrap<Ts>::type...> const&&,
                Us...
            >::type operator()(Us&&... vs) const&&
            {
                return detail::bound_front_impl(std::move(_f),
                    typename detail::make_index_pack<sizeof...(Ts)>::type(),
                    std::move(_args), std::forward<Us>(vs)...);
            }

            template <typename Archive>
            void serialize(Archive& ar, unsigned int const /*version*/)
            {
                ar & _f;
                ar & _args;
            }

            std::size_t get_function_address() const
            {
                return traits::get_function_address<
                        typename std::decay<F>::type
                    >::call(_f);
            }

            char const* get_function_annotation() const
            {
#if defined(HPX_HAVE_THREAD_DESCRIPTION)
                return traits::get_function_annotation<
                        typename std::decay<F>::type
                    >::call(_f);
#else
                return nullptr;
#endif
            }

#if HPX_HAVE_ITTNOTIFY != 0 && !defined(HPX_HAVE_APEX)
            util::itt::string_handle get_function_annotation_itt() const
            {
#if defined(HPX_HAVE_THREAD_DESCRIPTION)
                return traits::get_function_annotation_itt<
                        typename std::decay<F>::type
                    >::call(_f);
#else
                static util::itt::string_handle sh("bound_front");
                return sh;
#endif
            }
#endif

        private:
            typename std::decay<F>::type _f;
            util::tuple<typename util::decay_unwrap<Ts>::type...> _args;
        };
    }

    template <typename F, typename ...Ts>
    HPX_CONSTEXPR detail::bound_front<
        typename std::decay<F>::type,
        typename std::decay<Ts>::type...>
    bind_front(F&& f, Ts&&... vs) {
        typedef detail::bound_front<
            typename std::decay<F>::type,
            typename std::decay<Ts>::type...
        > result_type;

        return result_type(std::forward<F>(f), std::forward<Ts>(vs)...);
    }
}}

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace traits
{
    ///////////////////////////////////////////////////////////////////////////
#if defined(HPX_HAVE_THREAD_DESCRIPTION)
    template <typename F, typename ...Ts>
    struct get_function_address<util::detail::bound_front<F, Ts...> >
    {
        static std::size_t
            call(util::detail::bound_front<F, Ts...> const& f) noexcept
        {
            return f.get_function_address();
        }
    };

    ///////////////////////////////////////////////////////////////////////////
    template <typename F, typename ...Ts>
    struct get_function_annotation<util::detail::bound_front<F, Ts...> >
    {
        static char const*
            call(util::detail::bound_front<F, Ts...> const& f) noexcept
        {
            return f.get_function_annotation();
        }
    };

#if HPX_HAVE_ITTNOTIFY != 0 && !defined(HPX_HAVE_APEX)
    template <typename F, typename ...Ts>
    struct get_function_annotation_itt<util::detail::bound_front<F, Ts...> >
    {
        static util::itt::string_handle
            call(util::detail::bound_front<F, Ts...> const& f) noexcept
        {
            return f.get_function_annotation_itt();
        }
    };
#endif
#endif
}}

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace serialization
{
    // serialization of the bound_front object
    template <typename Archive, typename F, typename ...Ts>
    void serialize(
        Archive& ar
      , ::hpx::util::detail::bound_front<F, Ts...>& bound
      , unsigned int const version = 0)
    {
        bound.serialize(ar, version);
    }
}}

#endif
