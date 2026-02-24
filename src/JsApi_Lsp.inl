// =============================================================================
// JsApi_Lsp.inl
// JS API: LSP (Language Server Protocol) Integration
// Included by ScriptEngine.cpp
// =============================================================================

static duk_ret_t js_editor_lsp_start(duk_context *ctx) {
  const char *serverPath = duk_get_string(ctx, 0);
  const char *rootDir = duk_get_string(ctx, 1);
  if (!serverPath || !rootDir) {
    duk_push_boolean(ctx, false);
    return 1;
  }

  if (!g_lspClient) {
    g_lspClient = new LspClient();
  }

  std::wstring wServer = StringToWString(serverPath);
  std::wstring wRoot = StringToWString(rootDir);
  bool success = g_lspClient->Start(wServer, wRoot);
  duk_push_boolean(ctx, success);
  return 1;
}

static duk_ret_t js_editor_lsp_stop(duk_context *ctx) {
  if (g_lspClient) {
    g_lspClient->Stop();
  }
  return 0;
}

static duk_ret_t js_editor_lsp_request(duk_context *ctx) {
  const char *method = duk_get_string(ctx, 0);
  const char *params = duk_get_string(ctx, 1);
  if (!method || !params || !g_lspClient) {
    duk_push_number(ctx, -1);
    return 1;
  }

  int id = g_lspClient->SendRequest(method, params);
  duk_push_number(ctx, (double)id);
  return 1;
}

static duk_ret_t js_editor_lsp_notify(duk_context *ctx) {
  const char *method = duk_get_string(ctx, 0);
  const char *params = duk_get_string(ctx, 1);
  if (!method || !params || !g_lspClient) {
    return 0;
  }

  g_lspClient->SendNotification(method, params);
  return 0;
}

static duk_ret_t js_editor_lsp_get_response(duk_context *ctx) {
  int id = (int)duk_get_number(ctx, 0);
  if (!g_lspClient) {
    duk_push_string(ctx, "");
    return 1;
  }

  std::string resp = g_lspClient->GetResponse(id);
  duk_push_string(ctx, resp.c_str());
  return 1;
}

static duk_ret_t js_editor_lsp_get_diagnostics(duk_context *ctx) {
  if (!g_lspClient) {
    duk_push_string(ctx, "");
    return 1;
  }

  std::string diag = g_lspClient->GetDiagnostics();
  duk_push_string(ctx, diag.c_str());
  return 1;
}
