#include "Connection.h"
#include <sys/uio.h>
#include <sys/socket.h>
#include <iostream>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
void Connection::Start() {
  std::lock_guard<std::mutex> lock(mut);
  _event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
  good.store(true);
  close.store(false);
  _event.data.fd = _socket;
  argument_for_command.resize(0);
  parser.Reset();
  arg_remains = 0;
  offs = 0;
  readed_bytes = 0;
  feedbacks.clear();
}

// See Connection.h        good.store(true);
void Connection::OnError() {
  good.store(false);
  shutdown(_socket, SHUT_RDWR);
  command_to_execute.reset();
  argument_for_command.resize(0);
  parser.Reset();
  feedbacks.clear();
}

// See Connection.h
void Connection::OnClose() {
  if (!feedbacks.size()) {
    good.store(false);
    shutdown(_socket, SHUT_RDWR);
    command_to_execute.reset();
    argument_for_command.resize(0);
    parser.Reset();
    feedbacks.clear();
  } else {
    _event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    shutdown(_socket, SHUT_RD);
    command_to_execute.reset();
    argument_for_command.resize(0);
    parser.Reset();
    close.store(true);
  }
}

// See Connection.h
void Connection::DoRead() {
  std::lock_guard<std::mutex> lock(mut);
  int client_socket = _socket;
  command_to_execute = nullptr;
  try {
      int curr = -1;
      while ((curr= read(client_socket, client_buffer + readed_bytes, sizeof(client_buffer) - readed_bytes)) > 0) {
          _logger->debug("Got {} bytes from socket", readed_bytes);
          readed_bytes += curr;
          // Single block of data readed from the socket could trigger inside actions a multiple times,
          // for example:
          // - read#0: [<command1 start>]
          // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
          while (good.load() && readed_bytes > 0) {
              _logger->debug("Process {} bytes", readed_bytes);
              // There is no command yet
              if (!command_to_execute) {
                  std::size_t parsed = 0;
                  if (parser.Parse(client_buffer, readed_bytes, parsed)) {
                      // There is no command to be launched, continue to parse input stream
                      // Here we are, current chunk finished some command, process it
                      _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                      command_to_execute = parser.Build(arg_remains);
                      if (arg_remains > 0) {
                          arg_remains += 2;
                      }
                  }

                  // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                  // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                  if (parsed == 0) {
                      break;
                  } else {
                      std::memmove(client_buffer, client_buffer + parsed, readed_bytes - parsed);
                      readed_bytes -= parsed;
                  }
              }

              // There is command, but we still wait for argument to arrive...
              if (command_to_execute && arg_remains > 0) {
                  _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                  // There is some parsed command, and now we are reading argument
                  std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                  argument_for_command.append(client_buffer, to_read);

                  std::memmove(client_buffer, client_buffer + to_read, readed_bytes - to_read);
                  arg_remains -= to_read;
                  readed_bytes -= to_read;
              }

              // Thre is command & argument - RUN!
              if (command_to_execute && arg_remains == 0) {
                  _logger->debug("Start command execution");

                  std::string result;
                  command_to_execute->Execute(*pStorage, argument_for_command, result);

                  // Send response
                  result += "\r\n";
                  feedbacks.push_back(result);
                  // Prepare for the next command
                  command_to_execute.reset();
                  argument_for_command.resize(0);
                  parser.Reset();
              }
          } // while (readed_bytes)
      }
      if (!good.load() || readed_bytes == 0) {
          _logger->debug("Connection closed");
          OnClose();
      }
  } catch (std::runtime_error &ex) {
      _logger->error("Failed to process connection on descriptor {}: {}", client_socket, ex.what());
      OnClose();
  }
  if (!feedbacks.empty()) {
      _event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
  }
}

// See Connection.h
void Connection::DoWrite() {
  std::lock_guard<std::mutex> lock(mut);
  if (feedbacks.empty()) {
    return;
  }
  try {
        iovec dat[feedbacks.size()];
        for (int i = 0; i < feedbacks.size(); ++i) {
            dat[i].iov_len = feedbacks[i].size();
            dat[i].iov_base = &(feedbacks[i][0]);
        }
        dat[0].iov_len -= offs;
        dat[0].iov_base = &(feedbacks[0][offs]);
        std::size_t num = writev(_socket, dat, feedbacks.size());
        if (num <= 0) {
          OnError();
        } else {
          std::size_t wrote = offs + num;
          auto it = feedbacks.begin();
          while (it != feedbacks.end() && (*it).size() <= wrote) {
              wrote -= (*it).size();
              ++it;
          }
          feedbacks.erase(feedbacks.begin(), it);
          offs = wrote;
        }
        if (feedbacks.empty()) {
          if (close.load()) {
            OnClose();
          } else {
            _event.events = EPOLLIN;
          }
        }
    } catch (std::runtime_error &ex) {
        OnError();
    }
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina
