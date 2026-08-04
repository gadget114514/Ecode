#include "../include/LspClient.h"
#include <iostream>
#include <sstream>

enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };
void DebugLog(const std::string &msg, LogLevel level = LOG_INFO);

LspClient::LspClient() : m_nextRequestId(1) {}

LspClient::~LspClient() { Stop(); }

bool LspClient::Start(const std::wstring &serverPath,
                      const std::wstring &rootDir) {
  if (m_process && m_process->IsRunning())
    return false;

  m_process = std::make_unique<Process>();
  bool success = m_process->Start(serverPath, [this](const std::string &out) {
    this->OnProcessOutput(out);
  });

  if (success) {
    // Send 'initialize' request
    // Simplified root URI construction
    std::string rootUri = "file:///";
    for (wchar_t c : rootDir) {
      if (c == L'\\')
        rootUri += '/';
      else
        rootUri += (char)c;
    }

    std::string initParams =
        "{\"processId\":" + std::to_string(GetCurrentProcessId()) +
        ",\"rootUri\":\"" + rootUri + "\",\"capabilities\":{}}";
    SendRequest("initialize", initParams);
    SendNotification("initialized", "{}");
  }

  return success;
}

void LspClient::Stop() {
  if (m_process) {
    m_process->Stop();
  }
}

int LspClient::SendRequest(const std::string &method,
                           const std::string &paramsJson) {
  int id = m_nextRequestId++;
  std::string request = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
                        ",\"method\":\"" + method +
                        "\",\"params\":" + paramsJson + "}";

  std::string header =
      "Content-Length: " + std::to_string(request.length()) + "\r\n\r\n";
  DebugLog("LSP SendRequest: method=" + method + ", id=" + std::to_string(id), LOG_DEBUG); m_process->Write(header + request);
  return id;
}

void LspClient::SendNotification(const std::string &method,
                                 const std::string &paramsJson) {
  std::string notification = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method +
                             "\",\"params\":" + paramsJson + "}";

  std::string header =
      "Content-Length: " + std::to_string(notification.length()) + "\r\n\r\n";
  DebugLog("LSP SendNotification: method=" + method, LOG_DEBUG); m_process->Write(header + notification);
}

void LspClient::OnProcessOutput(const std::string &output) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_readBuffer += output;

  while (true) {
    size_t headerPos = m_readBuffer.find("Content-Length: ");
    if (headerPos == std::string::npos)
      break;

    size_t colonPos = m_readBuffer.find(":", headerPos);
    size_t endHeaderPos = m_readBuffer.find("\r\n\r\n", colonPos);
    if (endHeaderPos == std::string::npos)
      break;

    std::string lengthStr =
        m_readBuffer.substr(colonPos + 1, endHeaderPos - (colonPos + 1));
    int length = std::stoi(lengthStr);

    if (m_readBuffer.length() < endHeaderPos + 4 + length)
      break;

    std::string message = m_readBuffer.substr(endHeaderPos + 4, length);
    HandleMessage(message);

    m_readBuffer.erase(0, endHeaderPos + 4 + length);
  }
}

void LspClient::HandleMessage(const std::string &json) {
  // Very basic JSON parsing for id and method
  // In a real implementation, we'd use a proper JSON library
  DebugLog("LSP HandleMessage: " + json.substr(0, (std::min)((size_t)200, json.length())), LOG_DEBUG);
  try {
    if (json.find("\"id\":") != std::string::npos) {
      // It's a response
      size_t idPos = json.find("\"id\":");
      size_t valStart = json.find_first_of("0123456789", idPos);
      if (valStart != std::string::npos) {
        size_t valEnd = json.find_first_not_of("0123456789", valStart);
        size_t len = (valEnd == std::string::npos) ? std::string::npos : (valEnd - valStart);
        int id = std::stoi(json.substr(valStart, len));
        m_responses[id] = json;
      }
    } else if (json.find("\"method\":\"textDocument/publishDiagnostics\"") !=
               std::string::npos) {
      m_diagnostics = json;
    }
  } catch (const std::exception &e) {
    DebugLog("LSP HandleMessage - Exception: " + std::string(e.what()), LOG_ERROR);
  } catch (...) {
    DebugLog("LSP HandleMessage - Unknown exception", LOG_ERROR);
  }
}

std::string LspClient::GetResponse(int requestId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_responses.find(requestId);
  if (it != m_responses.end()) {
    return it->second;
  }
  return "";
}

std::string LspClient::GetDiagnostics() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_diagnostics;
}
