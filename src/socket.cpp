/**
 * php-asio/socket.cpp
 *
 * @author CismonX<admin@cismon.net>
 */

#include "socket.hpp"
#include "future.hpp"
#include "stream_descriptor.hpp"

namespace Asio
{
    template <typename Protocol>
    zval* Socket<Protocol>::connect_handler(const boost::system::error_code& error,
        zval* callback, zval* argument)
    {
        PHP_ASIO_INVOKE_CALLBACK_START(3)
            PHP_ASIO_INVOKE_CALLBACK();
        PHP_ASIO_INVOKE_CALLBACK_END();
        CORO_RETURN_NULL();
    }

    template <typename Protocol> template <typename, typename>
    zval* Socket<Protocol>::read_handler(const boost::system::error_code& error,
        size_t length, std::vector<char>* buffer, zval* callback, zval* argument)
    {
        auto str_buffer = zend_string_init(buffer->data(), length, 0);
        delete buffer;
        PHP_ASIO_INVOKE_CALLBACK_START(4)
            ZVAL_STR(&arguments[1], str_buffer);
            PHP_ASIO_INVOKE_CALLBACK();
        PHP_ASIO_INVOKE_CALLBACK_END();
        CORO_RETURN(ZVAL_STR, str_buffer);
    }

    template <typename Protocol>
    zval* Socket<Protocol>::write_handler(const boost::system::error_code& error,
        size_t length, std::string* buffer, zval* callback, zval* argument)
    {
        delete buffer;
        PHP_ASIO_INVOKE_CALLBACK_START(4)
            ZVAL_LONG(&arguments[1], static_cast<zend_long>(length));
            PHP_ASIO_INVOKE_CALLBACK();
        PHP_ASIO_INVOKE_CALLBACK_END();
        CORO_RETURN(ZVAL_LONG, static_cast<zend_long>(length));
    }

    template <>
    zval* UdpSocket::recv_handler(const boost::system::error_code& error,
        size_t length, std::vector<char>* buffer, udp::endpoint* endpoint,
        zval* callback, zval* argument)
    {
        auto str_buffer = zend_string_init(buffer->data(), length, 0);
        delete buffer;
#ifdef ENABLE_COROUTINE
        last_addr_<> = endpoint->address().to_string();
        last_port_<> = endpoint->port();
#endif // ENABLE_COROUTINE
        PHP_ASIO_INVOKE_CALLBACK_START(6)
            ZVAL_STR(&arguments[1], str_buffer);
#ifdef ENABLE_COROUTINE
            ZVAL_STRING(&arguments[2], last_addr_<>.data());
            ZVAL_LONG(&arguments[3], static_cast<zend_long>(last_port_<>));
#else
            ZVAL_STRING(&arguments[2], endpoint->address().to_string().data());
            ZVAL_LONG(&arguments[3], static_cast<zend_long>(endpoint->port()));
#endif // ENABLE_COROUTINE
            PHP_ASIO_INVOKE_CALLBACK();
            zval_ptr_dtor(&arguments[2]);
        PHP_ASIO_INVOKE_CALLBACK_END();
        delete endpoint;
        CORO_RETURN(ZVAL_STR, str_buffer);
    }

    template <>
    zval* UdgSocket::recv_handler(const boost::system::error_code& error,
        size_t length, std::vector<char>* buffer, udg::endpoint* endpoint,
        zval* callback, zval* argument)
    {
        auto str_buffer = zend_string_init(buffer->data(), length, 0);
        delete buffer;
#ifdef ENABLE_COROUTINE
        last_path_<> = endpoint->path();
#endif // ENABLE_COROUTINE
        PHP_ASIO_INVOKE_CALLBACK_START(5)
            ZVAL_STR(&arguments[1], str_buffer);
#ifdef ENABLE_COROUTINE
            ZVAL_STRING(&arguments[2], last_path_<>.data());
#else
            ZVAL_STRING(&arguments[2], endpoint->path().data());
#endif // ENABLE_COROUTINE
            PHP_ASIO_INVOKE_CALLBACK();
            zval_ptr_dtor(&arguments[2]);
        PHP_ASIO_INVOKE_CALLBACK_END();
        delete endpoint;
        CORO_RETURN(ZVAL_STR, str_buffer);
    }

    template <typename Protocol> template <typename P, ENABLE_IF_INET(P)>
    P3_METHOD(Socket<Protocol>, open_inet)
    {
        zend_bool inet6;
        ZEND_PARSE_PARAMETERS_START(1, 1)
            Z_PARAM_BOOL(inet6)
        ZEND_PARSE_PARAMETERS_END();
        boost::system::error_code ec;
        socket_.open(inet6 ? Protocol::v6() : Protocol::v4(), ec);
        RETVAL_EC(ec);
    }

    template <typename Protocol> template <typename P, ENABLE_IF_LOCAL(P)>
    P3_METHOD(Socket<Protocol>, open_local)
    {
        boost::system::error_code ec;
        socket_.open(Protocol(), ec);
        RETVAL_EC(ec);
    }

    template <typename Protocol> template <typename P, ENABLE_IF_INET(P)>
    P3_METHOD(Socket<Protocol>, assign_inet)
    {
        PHP_ASIO_INET_ASSIGN(socket_, Protocol);
    }

    template <typename Protocol> template <typename P, ENABLE_IF_LOCAL(P)>
    P3_METHOD(Socket<Protocol>, assign_local)
    {
        PHP_ASIO_LOCAL_ASSIGN(socket_, Protocol);
    }

    template <typename Protocol> template <typename P, ENABLE_IF_INET(P)>
    P3_METHOD(Socket<Protocol>, bind_inet)
    {
        zend_string* address;
        zend_long port_num;
        ZEND_PARSE_PARAMETERS_START(2, 2);
            Z_PARAM_STR(address)
            Z_PARAM_LONG(port_num)
        ZEND_PARSE_PARAMETERS_END();
        boost::system::error_code ec;
        socket_.bind({ boost::asio::ip::address::from_string(ZSTR_VAL(address)),
            static_cast<unsigned short>(port_num) }, ec);
        RETVAL_EC(ec);
    }

    template <typename Protocol> template <typename P, ENABLE_IF_LOCAL(P)>
    P3_METHOD(Socket<Protocol>, bind_local)
    {
        zend_string* socket_path;
        ZEND_PARSE_PARAMETERS_START(1, 1);
            Z_PARAM_STR(socket_path)
        ZEND_PARSE_PARAMETERS_END();
        namespace fs = boost::filesystem;
        if (fs::status(ZSTR_VAL(socket_path)).type() == fs::socket_file)
            fs::remove(ZSTR_VAL(socket_path));
        boost::system::error_code ec;
        socket_.bind({ ZSTR_VAL(socket_path) }, ec);
        RETVAL_EC(ec);
    }

    /* {{{ proto Future TcpSocket::connect(string address, int port,
     *               [callable callback], [mixed argument]);
     * Asynchronously connect to a remote endpoint. */
    template <>
    P3_METHOD(TcpSocket, connect)
    {
        zend_string* address;
        zend_long port;
        zval* callback = nullptr;
        zval* argument = nullptr;
        ZEND_PARSE_PARAMETERS_START(2, 4)
            Z_PARAM_STR(address)
            Z_PARAM_LONG(port)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(callback)
            Z_PARAM_ZVAL(argument)
        ZEND_PARSE_PARAMETERS_END();
        PHP_ASIO_FUTURE_INIT();
        future->on_resolve<NOARG>(boost::bind(
            &Socket::connect_handler, this, _1, STRAND_UNWRAP(), args));
        socket_.async_connect({ boost::asio::ip::address::from_string(ZSTR_VAL(address)),
            static_cast<unsigned short>(port) }, STRAND_RESOLVE(ASYNC_HANDLER_SINGLE_ARG));
        FUTURE_RETURN();
    }
    /* }}} */

    /* {{{ proto Future UnixSocket::connect(string path, [callable callback], [mixed argument]);
     * Asynchronously connect to a remote endpoint. */
    template <>
    P3_METHOD(UnixSocket, connect)
    {
        zend_string* path;
        zval* callback = nullptr;
        zval* argument = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 3)
            Z_PARAM_STR(path)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(callback)
            Z_PARAM_ZVAL(argument)
        ZEND_PARSE_PARAMETERS_END();
        PHP_ASIO_FUTURE_INIT();
        future->on_resolve<NOARG>(boost::bind(
            &Socket::connect_handler, this, _1, STRAND_UNWRAP(), args));
        socket_.async_connect({ ZSTR_VAL(path) }, STRAND_RESOLVE(ASYNC_HANDLER_SINGLE_ARG));
        FUTURE_RETURN();
    }
    /* }}} */

    template <typename Protocol> template <typename, typename>
    P3_METHOD(Socket<Protocol>, read)
    {
        zend_long length;
        zend_bool read_some = 1;
        zval* callback = nullptr;
        zval* argument = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 4)
            Z_PARAM_LONG(length)
            Z_PARAM_OPTIONAL
            Z_PARAM_BOOL(read_some)
            Z_PARAM_ZVAL(callback)
            Z_PARAM_ZVAL(argument)
        ZEND_PARSE_PARAMETERS_END();
        if (UNEXPECTED(length < 0)) {
            PHP_ASIO_ERROR(E_WARNING, "Non-negative integer expected.");
            RETURN_NULL();
        }
        const auto size = static_cast<size_t>(length);
        auto buffer_container = new std::vector<char>(size);
        auto buffer = boost::asio::buffer(*buffer_container, size);
        PHP_ASIO_FUTURE_INIT();
        future->on_resolve<size_t>(boost::bind(
            &Socket::read_handler, this, _1, _2, buffer_container, STRAND_UNWRAP(), args));
        if (read_some)
            socket_.async_read_some(buffer, STRAND_RESOLVE(ASYNC_HANDLER_DOUBLE_ARG(size_t)));
        else
            async_read(socket_, buffer, STRAND_RESOLVE(ASYNC_HANDLER_DOUBLE_ARG(size_t)));
        FUTURE_RETURN();
    }

    template <typename Protocol> template <typename, typename>
    P3_METHOD(Socket<Protocol>, write)
    {
        zend_string* data;
        zend_bool write_some = 0;
        zval* callback = nullptr;
        zval* argument = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 4)
            Z_PARAM_STR(data)
            Z_PARAM_OPTIONAL
            Z_PARAM_BOOL(write_some)
            Z_PARAM_ZVAL(callback)
            Z_PARAM_ZVAL(argument)
        ZEND_PARSE_PARAMETERS_END();
        auto buffer = new std::string(ZSTR_VAL(data), ZSTR_LEN(data));
        PHP_ASIO_FUTURE_INIT();
        future->on_resolve<size_t>(boost::bind(
            &Socket::write_handler, this, _1, _2, buffer, STRAND_UNWRAP(), argument));
        if (write_some)
            socket_.async_write_some(boost::asio::buffer(*buffer),
                STRAND_RESOLVE(ASYNC_HANDLER_DOUBLE_ARG(size_t)));
        else
            async_write(socket_, boost::asio::buffer(*buffer),
                STRAND_RESOLVE(ASYNC_HANDLER_DOUBLE_ARG(size_t)));
        FUTURE_RETURN();
    }

    template <typename Protocol> template <typename, typename>
    P3_METHOD(Socket<Protocol>, recvFrom)
    {
        zend_long length;
        zval* callback = nullptr;
        zval* argument = nullptr;
        ZEND_PARSE_PARAMETERS_START(1, 3);
            Z_PARAM_LONG(length)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(callback)
            Z_PARAM_ZVAL(argument)
        ZEND_PARSE_PARAMETERS_END();
        if (UNEXPECTED(length < 0)) {
            PHP_ASIO_ERROR(E_WARNING, "Non-negative integer expected.");
            RETURN_NULL();
        }
        const auto size = static_cast<size_t>(length);
        auto buffer_container = new std::vector<char>(size);
        auto buffer = boost::asio::buffer(*buffer_container, size);
        auto endpoint = new typename Protocol::endpoint;
        PHP_ASIO_FUTURE_INIT();
        future->on_resolve<size_t>(boost::bind(&Socket::recv_handler,
            this, _1, _2, buffer_container, endpoint, STRAND_UNWRAP(), args));
        socket_.async_receive_from(buffer, *endpoint, STRAND_RESOLVE(ASYNC_HANDLER_DOUBLE_ARG(size_t)));
        FUTURE_RETURN();
    }

    /* {{{ proto Future UdpSocket::sendTo(string data, string address, int port,
     *               [callable callback], [mixed argument]);
    * Send asynchronously to UDP socket. */
    template <> template <>
    P3_METHOD(UdpSocket, sendTo)
    {
        zend_string* data;
        zend_string* address;
        zend_long port;
        zval* callback = nullptr;
        zval* argument = nullptr;
        ZEND_PARSE_PARAMETERS_START(3, 5)
            Z_PARAM_STR(data)
            Z_PARAM_STR(address)
            Z_PARAM_LONG(port)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(callback)
            Z_PARAM_ZVAL(argument)
        ZEND_PARSE_PARAMETERS_END();
        const udp::endpoint endpoint(boost::asio::ip::address::from_string(ZSTR_VAL(address)),
            static_cast<unsigned short>(port));
        const auto buffer = new std::string(ZSTR_VAL(data), ZSTR_LEN(data));
        PHP_ASIO_FUTURE_INIT();
        future->on_resolve<size_t>(boost::bind(
            &Socket::write_handler, this, _1, _2, buffer, STRAND_UNWRAP(), args));
        socket_.async_send_to(boost::asio::buffer(*buffer), endpoint,
            STRAND_RESOLVE(ASYNC_HANDLER_DOUBLE_ARG(size_t)));
        FUTURE_RETURN();
    }
    /* }}} */

    /* {{{ proto Future UdgSocket::sendTo(string data, string path,
     *               [callable callback], [mixed argument]);
     * Send asynchronously to UNIX datagram socket. */
    template <> template <>
    P3_METHOD(UdgSocket, sendTo)
    {
        zend_string* data;
        zend_string* path;
        zval* callback = nullptr;
        zval* argument = nullptr;
        ZEND_PARSE_PARAMETERS_START(2, 4)
            Z_PARAM_STR(data)
            Z_PARAM_STR(path)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(callback)
            Z_PARAM_ZVAL(argument)
        ZEND_PARSE_PARAMETERS_END();
        const auto buffer = new std::string(ZSTR_VAL(data));
        PHP_ASIO_FUTURE_INIT();
        future->on_resolve<size_t>(boost::bind(
            &Socket::write_handler, this, _1, _2, buffer, STRAND_UNWRAP(), args));
        socket_.async_send_to(boost::asio::buffer(*buffer), { ZSTR_VAL(path) },
            STRAND_RESOLVE(ASYNC_HANDLER_DOUBLE_ARG(size_t)));
        FUTURE_RETURN();
    }
    /* }}} */

    template <> template <>
    P3_METHOD(TcpSocket, remoteAddr) const
    {
        RETVAL_STRING(socket_.remote_endpoint().address().to_string().c_str());
    }

    template <> template <>
    P3_METHOD(TcpSocket, remotePort) const
    {
        RETVAL_LONG(static_cast<zend_long>(socket_.remote_endpoint().port()));
    }

    template <> template <>
    P3_METHOD(UnixSocket, remotePath) const
    {
        RETVAL_STRING(socket_.remote_endpoint().path().c_str());
    }

    template <> template <>
    P3_METHOD(UdpSocket, remoteAddr) const
    {
#ifdef ENABLE_COROUTINE
        RETVAL_STRING(last_addr_<>.data());
#else
        PHP_ASIO_ERROR(E_WARNING, "Not available without coroutine support.");
        RETVAL_NULL();
#endif // ENABLE_COROUTINE
    }

    template <> template <>
    P3_METHOD(UdpSocket, remotePort) const
    {
#ifdef ENABLE_COROUTINE
        RETVAL_LONG(static_cast<zend_long>(last_port_<>));
#else
        PHP_ASIO_ERROR(E_WARNING, "Not available without coroutine support.");
        RETVAL_NULL();
#endif // ENABLE_COROUTINE
    }

    template <> template <>
    P3_METHOD(UdgSocket, remotePath) const
    {
#ifdef ENABLE_COROUTINE
        RETVAL_STRING(last_path_<>.data());
#else
        PHP_ASIO_ERROR(E_WARNING, "Not available without coroutine support.");
        RETVAL_NULL();
#endif // ENABLE_COROUTINE
    }

    template <typename Protocol>
    P3_METHOD(Socket<Protocol>, available) const
    {
        zval* error = nullptr;
        ZEND_PARSE_PARAMETERS_START(0, 1)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(error)
        ZEND_PARSE_PARAMETERS_END();
        boost::system::error_code ec;
        auto bytes = socket_.available(ec);
        if (error) {
            ZVAL_DEREF(error);
            ZVAL_LONG(error, static_cast<zend_long>(ec.value()));
        }
        RETVAL_LONG(static_cast<zend_long>(bytes));
    }

    template <typename Protocol>
    P3_METHOD(Socket<Protocol>, atMark) const
    {
        zval* error = nullptr;
        ZEND_PARSE_PARAMETERS_START(0, 1)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(error)
        ZEND_PARSE_PARAMETERS_END();
        boost::system::error_code ec;
        auto at_mark = socket_.at_mark(ec);
        if (error) {
            ZVAL_DEREF(error);
            ZVAL_LONG(error, static_cast<zend_long>(ec.value()));
        }
        RETVAL_BOOL(at_mark);
    }

    template <typename Protocol>
    P3_METHOD(Socket<Protocol>, close)
    {
        boost::system::error_code ec;
        socket_.cancel(ec);
        if (!ec) {
            socket_.close(ec);
            if (!ec)
                PHP_ASIO_OBJ_DTOR(Socket);
        }
        RETVAL_EC(ec);
    }

    template <typename Protocol>
    zend_class_entry* Socket<Protocol>::class_entry;

    template <typename Protocol>
    zend_object_handlers Socket<Protocol>::handlers;

#ifdef ENABLE_COROUTINE

    template <typename Protocol> template <typename, typename>
    thread_local std::string Socket<Protocol>::last_addr_;

    template <typename Protocol> template <typename, typename>
    thread_local unsigned short Socket<Protocol>::last_port_;

    template <typename Protocol> template <typename, typename>
    thread_local std::string Socket<Protocol>::last_path_;

#endif // ENABLE_COROUTINE

    template class Socket<tcp>;
    template P3_METHOD(TcpSocket, open_inet);
    template P3_METHOD(TcpSocket, assign_inet);
    template P3_METHOD(TcpSocket, bind_inet);
    template zval* TcpSocket::read_handler(const boost::system::error_code&,
        size_t, std::vector<char>*, zval*, zval*);
    template P3_METHOD(TcpSocket, read);
    template P3_METHOD(TcpSocket, write);

    template class Socket<unix>;
    template P3_METHOD(UnixSocket, open_local);
    template P3_METHOD(UnixSocket, assign_local);
    template P3_METHOD(UnixSocket, bind_local);
    template zval* UnixSocket::read_handler(const boost::system::error_code&,
        size_t, std::vector<char>*, zval*, zval*);
    template P3_METHOD(UnixSocket, read);
    template P3_METHOD(UnixSocket, write);

    template class Socket<udp>;
    template P3_METHOD(UdpSocket, open_inet);
    template P3_METHOD(UdpSocket, assign_inet);
    template P3_METHOD(UdpSocket, bind_inet);
    template P3_METHOD(UdpSocket, recvFrom);

    template class Socket<udg>;
    template P3_METHOD(UdgSocket, open_local);
    template P3_METHOD(UdgSocket, assign_local);
    template P3_METHOD(UdgSocket, bind_local);
    template P3_METHOD(UdgSocket, recvFrom);
}