#include "Socket.hpp"
/*
* Liam Ashdown
* Copyright (C) 2019
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <boost/lexical_cast.hpp>

#include "Socket.hpp"
#include "Utility/UtiObjectGuard.hpp"

namespace SteerStone { namespace Core { namespace Network {

    /// Constructor
    /// @p_Service : Socket to pass
    /// @p_CloseHandler : Custom Handler to handle our function
    Socket::Socket(boost::asio::io_service& p_Service, std::function<void(Socket*)> p_CloseHandler)
        : m_WriteState(WriteState::Idle), m_ReadState(ReadState::Idle), m_Socket(p_Service),
        m_CloseHandler(std::move(p_CloseHandler)), m_OutBufferFlushTimer(p_Service), m_Address("0.0.0.0") {}

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    /// Open Socket to read incoming packets
    bool Socket::Open()
    {
        try
        {
            const_cast<std::string&>(m_Address)         = m_Socket.remote_endpoint().address().to_string();
            const_cast<std::string&>(m_RemoteEndPoint)  = boost::lexical_cast<std::string>(m_Socket.remote_endpoint());
        }
        catch (boost::system::error_code&)
        {
            LOG_ERROR("Socket", "Failed to initialize socket address");
            return false;
        }

        m_OutBuffer.reset(new PacketBuffer);
        m_InBuffer.reset(new PacketBuffer);

        StartAsyncRead();
    }
    /// Close our socket to stop recieving incoming packets
    void Socket::CloseSocket()
    {
        Utils::ObjectGuard l_Guard(this);

        if (IsClosed())
            return;

        boost::system::error_code l_ErrorCode;
        m_Socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, l_ErrorCode);
        m_Socket.close();

        if (m_CloseHandler)
            m_CloseHandler(this);
    }
    /// Close our socket to stop recieving incoming packets
    bool Socket::IsClosed() const
    {
        return !m_Socket.is_open();
    }
    /// Check if our socket can be deleted
    bool Socket::Deletable() const
    {
        return IsClosed();
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    /// Read - Read the packet
    /// @p_Buffer : Buffer which holds the data
    /// @p_Length : The length of the data
    bool Socket::Read(char* p_Buffer, std::size_t const& p_Length)
    {
        if (ReadLengthRemaining() < p_Length)
            return false;

        m_InBuffer->Read(p_Buffer, p_Length);

        return true;
    }
    /// Skip parts of the packet
    /// @p_Length : The length of the data to skip
    void Socket::ReadSkip(std::size_t const& p_Length)
    {
        m_InBuffer->Read(nullptr, p_Length);
    }
    /// Write the data to be sent
    /// @p_Buffer : Buffer which holds the data
    /// @p_Length : The length of the data
    void Socket::Write(const char* p_Buffer, std::size_t const& p_Length)
    {
        Utils::ObjectGuard l_Guard(this);

        /// Write the header
        m_OutBuffer->Write(p_Buffer, p_Length);

        /// Flush data if need
        if (m_WriteState == WriteState::Idle)
            StartWriteFlushTimer();
    }
    /// Get the total read length of the packet
    std::size_t const Socket::ReadLength()
    {
        return m_InBuffer->ReadLength();
    }
    /// Get the length remaining to read
    std::size_t const Socket::ReadLengthRemaining()
    {
        return m_InBuffer->ReadLengthRemaining();
    }

    /// Get our AsioSocket
    boost::asio::ip::tcp::socket& Socket::GetAsioSocket()
    {
        return m_Socket;
    }
    /// Get our EndPoint
    std::string const& Socket::GetRemoteEndpoint()
    {
        return m_RemoteEndPoint;
    }
    /// Get our Remote Address
    std::string const& Socket::GetRemoteAddress()
    {
        return m_Address;
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    /// Get the current read position
    uint8 const* Socket::InPeak()
    {
        return &m_InBuffer->m_Buffer[m_InBuffer->m_ReadPosition];
    }
    /// ForceFlushOut - Send our current data in our buffer
    /// If the write state is idle, this will do nothing, which is correct
    /// If the write state is sending, this will do nothing, which is correct
    /// If the write state is buffering, this will cancel the running timer, which will immediately trigger FlushOut()
    void Socket::ForceFlushOut()
    {
        m_OutBufferFlushTimer.cancel();
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    /// Read incoming packets
    void Socket::StartAsyncRead()
    {
        if (IsClosed())
        {
            m_ReadState = ReadState::Idle;
            return;
        }

        std::shared_ptr<Socket> l_Ptr = Shared<Socket>();
        m_ReadState = ReadState::Reading;
        m_Socket.async_read_some(boost::asio::buffer(&m_InBuffer->m_Buffer[m_InBuffer->m_WritePosition], m_InBuffer->m_Buffer.size() - m_InBuffer->m_WritePosition),
            make_custom_alloc_handler(m_allocator,
                [l_Ptr](boost::system::error_code const& p_ErrorCode, std::size_t const& p_Length) { l_Ptr->OnRead(p_ErrorCode, p_Length); }));
    }
    /// OnRead - Handle the incoming packet
    /// @p_Error : Error code
    /// @p_Length : Length of failed buffer
    void Socket::OnRead(const boost::system::error_code& p_ErrorCode, const std::size_t& p_Length)
    {
        if (p_ErrorCode)
        {
            m_ReadState = ReadState::Idle;
            OnError(p_ErrorCode);
            return;
        }

        if (IsClosed())
        {
            m_ReadState = ReadState::Idle;
            return;
        }

        m_InBuffer->m_WritePosition += p_Length;

        const size_t l_Available = m_Socket.available();

        /// If there is still data to read, increase the buffer size and do so (if necessary)
        if (l_Available > 0 && (p_Length + l_Available) > m_InBuffer->m_Buffer.size())
        {
            m_InBuffer->m_Buffer.resize(m_InBuffer->m_Buffer.size() + l_Available);
            StartAsyncRead();
            return;
        }

        if (!ProcessIncomingData())
        {
            /// This errno is set when there is not enough buffer data available to either complete a header, or the packet length
            /// specified in the header goes past what we've read.  in this case, we will reset the buffer with the remaining data
            if (errno == EBADMSG)
            {
                const std::size_t l_BytesRemaining = m_InBuffer->m_WritePosition - m_InBuffer->m_ReadPosition;

                ::memmove(&m_InBuffer->m_Buffer[0], &m_InBuffer->m_Buffer[m_InBuffer->m_ReadPosition], l_BytesRemaining);

                m_InBuffer->m_ReadPosition = 0;
                m_InBuffer->m_WritePosition = l_BytesRemaining;

                StartAsyncRead();
            }
            else
                if (!IsClosed())
                    CloseSocket();

            return;
        }

        /// Reset to read next packet
        m_InBuffer->m_WritePosition = 0;
        m_InBuffer->m_ReadPosition = 0;

        StartAsyncRead();
    }
    /// OnWriteComplete - Finished sending out our buffer
    /// @p_Error : Error code
    /// @p_Length : Length of failed buffer
    void Socket::OnWriteComplete(boost::system::error_code const& p_ErrorCode, std::size_t const& p_Length)
    {
        /// We must check this before locking the mutex because the connection will be closed,
        /// which leads to a locked mutex being destroyed.  not good!
        if (p_ErrorCode)
        {
            OnError(p_ErrorCode);
            return;
        }

        if (IsClosed())
        {
            m_WriteState = WriteState::Idle;
            return;
        }

        Utils::ObjectGuard l_Guard(this);

        assert(m_WriteState == WriteState::Sending);
        assert(p_Length <= m_OutBuffer->m_WritePosition);

        /// If there is data left to write, move it to the start of the buffer
        if (p_Length < m_OutBuffer->m_WritePosition)
        {
            memcpy(&(m_OutBuffer->m_Buffer[0]), &(m_OutBuffer->m_Buffer[p_Length]), (m_OutBuffer->m_WritePosition - p_Length) * sizeof(m_OutBuffer->m_Buffer[0]));
            m_OutBuffer->m_WritePosition -= p_Length;
        }
        /// If not, reset the write pointer
        else
            m_OutBuffer->m_WritePosition = 0;

        std::shared_ptr<Socket> l_Ptr = Shared<Socket>();
        /// If there is any data to write, do so immediately
        if (m_OutBuffer->m_WritePosition > 0)
            m_Socket.async_write_some(boost::asio::buffer(m_OutBuffer->m_Buffer, m_OutBuffer->m_WritePosition),
                make_custom_alloc_handler(m_allocator,
                    [l_Ptr](boost::system::error_code const& p_ErrorCode, std::size_t const& p_Length) { l_Ptr->OnWriteComplete(p_ErrorCode, p_Length); }));
        else
            m_WriteState = WriteState::Idle;
    }
    /// Begin to send out our data in our buffer
    void Socket::FlushOut()
    {
        /// If the socket is closed, silently fail
        if (IsClosed())
        {
            m_WriteState = WriteState::Idle;
            return;
        }

        Utils::ObjectGuard l_Guard(this);

        assert(m_WriteState == WriteState::Buffering);

        /// At this point we are guarunteed that there is data to send in the primary buffer.  send it.
        m_WriteState = WriteState::Sending;

        std::shared_ptr<Socket> l_Ptr = Shared<Socket>();
        m_Socket.async_write_some(boost::asio::buffer(m_OutBuffer->m_Buffer, m_OutBuffer->m_WritePosition),
            make_custom_alloc_handler(m_allocator,
                [l_Ptr](boost::system::error_code const& p_ErrorCode, std::size_t const& p_Length) { l_Ptr->OnWriteComplete(p_ErrorCode, p_Length); }));
    }
    /// Start the time to send out our data in interval
    void Socket::StartWriteFlushTimer()
    {
        if (m_WriteState == WriteState::Buffering)
            return;

        /// If the socket is closed, silently fail
        if (IsClosed())
        {
            m_WriteState = WriteState::Idle;
            return;
        }

        m_WriteState = WriteState::Buffering;

        std::shared_ptr<Socket> l_Ptr = Shared<Socket>();
        m_OutBufferFlushTimer.expires_from_now(boost::posix_time::milliseconds(int32(m_BufferTimeout)));
        m_OutBufferFlushTimer.async_wait([l_Ptr](const boost::system::error_code& error) { l_Ptr->FlushOut(); });
    }
    /// Catch an error if packet is corrupted
    /// @p_Error : Error code
    void Socket::OnError(const boost::system::error_code& p_Error)
    {
        if (!IsClosed())
            CloseSocket();
    }

}   ///< namespace Network
}   ///< namespace Core
}   ///< namespace Steerstone