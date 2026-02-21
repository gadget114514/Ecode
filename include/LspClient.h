#pragma once
#include "Process.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>


class LspClient {
public:
  LspClient();
  ~LspClient();

  bool Start(const std::wstring &serverPath, const std::wstring &rootDir);
  void Stop();
  bool IsRunning() const { return m_process && m_process->IsRunning(); }

  int SendRequest(const std::string &method, const std::string &paramsJson);
  void SendNotification(const std::string &method,
                        const std::string &paramsJson);

  // Get the latest response for a request ID
  std::string GetResponse(int requestId);

  // Get aggregated diagnostics
  std::string GetDiagnostics();

private:
  void OnProcessOutput(const std::string &output);
  void HandleMessage(const std::string &json);

  std::unique_ptr<Process> m_process;
  int m_nextRequestId;
  std::string m_readBuffer;

  std::map<int, std::string> m_responses;
  std::string m_diagnostics; // Global diagnostics state (simplified)
};
