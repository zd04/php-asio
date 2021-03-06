/**
 * php-asio/timer.cpp
 *
 * @author CismonX<admin@cismon.net>
 */

#include <boost/date_time.hpp>

#include "timer.hpp"
#include "future.hpp"

namespace asio
{
    zval* timer::handler(const boost::system::error_code& error,
        zval* callback, zval* argument)
    {
        PHP_ASIO_INVOKE_CALLBACK_START(3)
            PHP_ASIO_INVOKE_CALLBACK();
        PHP_ASIO_INVOKE_CALLBACK_END();
        CORO_RETURN_NULL();
    }

    P3_METHOD(timer, expiresFromNow)
    {
        zend_long duration;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_LONG(duration)
        ZEND_PARSE_PARAMETERS_END();
        boost::system::error_code ec;
        timer_.expires_from_now(boost::posix_time::millisec(duration), ec);
        RETVAL_EC(ec);
    }

    P3_METHOD(timer, expiresAt)
    {
        zend_long timestamp;
        ZEND_PARSE_PARAMETERS_START(1, 2)
            Z_PARAM_LONG(timestamp)
        ZEND_PARSE_PARAMETERS_END();
        boost::system::error_code ec;
        timer_.expires_at(boost::posix_time::from_time_t(timestamp / 1000) +
            boost::posix_time::millisec(timestamp % 1000), ec);
        RETVAL_EC(ec);
    }

    P3_METHOD(timer, wait)
    {
        zval* callback = nullptr;
        zval* argument = nullptr;
        ZEND_PARSE_PARAMETERS_START(0, 2)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(callback)
            Z_PARAM_ZVAL(argument)
        ZEND_PARSE_PARAMETERS_END();
        PHP_ASIO_FUTURE_INIT();
        future->on_resolve<NOARG>(boost::bind(&timer::handler, this, _1, cb, args));
        timer_.async_wait(STRAND_RESOLVE(ASYNC_HANDLER_SINGLE_ARG));
        FUTURE_RETURN();
    }

    P3_METHOD(timer, cancel)
    {
        boost::system::error_code ec;
        timer_.cancel(ec);
        RETVAL_EC(ec);
    }

    PHP_ASIO_CE_DEFINE(timer);
}
