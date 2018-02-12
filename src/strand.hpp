/**
 * php-asio/include/strand.hpp
 *
 * @author CismonX<admin@cismon.net>
 */

#pragma once

#include "common.hpp"
#include "base.hpp"

#ifdef ENABLE_STRAND
namespace Asio
{
    /**
     * Wrapper for Boost.Asio strand.
     * Provides serialised handler execution.
     */
    class Strand : public Base
    {
        /**
         * Boost.Asio strand instance.
         */
        boost::asio::strand strand_;

    public:
        /**
         * Constructor.
         * @param io_service : I/O service for strand
         */
        explicit Strand(
            boost::asio::io_service& io_service
        ) : Base(io_service), strand_(io_service) {}

        /**
         * Deleted default constructor.
         */
        Strand() = delete;

        /**
         * Deleted copy constructor.
         */
        Strand(const Strand&) = delete;

        /* {{{ proto void Strand::dispatch(callable callback, [mixed argument]);
         * Request the strand to invoke the given handler. */
        P3_METHOD_DECLARE(dispatch);
        /* }}} */

        /* {{{ proto void Strand::post(callable callback, [mixed argument]);
         * Request the strand to invoke the given handler. */
        P3_METHOD_DECLARE(post);
        /* }}} */

        /* {{{ proto bool Strand::runningInThisThread(void);
         * Determine whether the strand is running in the current thread. */
        P3_METHOD_DECLARE(runningInThisThread) const;
        /* }}} */

        /* {{{ proto WrappedHandler wrap([callable callback]);
         * Create a new handler that automatically dispatches the wrapped handler on the strand. */
        P3_METHOD_DECLARE(wrap);
        /* }}} */

        /* {{{ proto int destroy(void);
         * Destroy current strand object. */
        P3_METHOD_DECLARE(destroy);
        /* }}} */

        PHP_ASIO_CE_DECLARE();
    };
}
#endif // ENABLE_STRAND