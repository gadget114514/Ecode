#include "../src/AIProvider.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

void TestAIRequestDefaults() {
    AIRequest req;
    VERIFY(req.model.empty(), "default model should be empty");
    VERIFY(req.systemPrompt.empty(), "default systemPrompt should be empty");
    VERIFY(req.userPrompt.empty(), "default userPrompt should be empty");
    VERIFY(req.temperature == 0.7, "default temperature should be 0.7");
    VERIFY(req.maxTokens == 4096, "default maxTokens should be 4096");
    VERIFY(req.attachments.empty(), "default attachments should be empty");
    VERIFY(req.extraParams.empty(), "default extraParams should be empty");
    std::cout << "Test Passed: AIRequest Defaults" << std::endl;
}

void TestAIRequestFullData() {
    AIRequest req;
    req.model = "gpt-4.1";
    req.systemPrompt = "You are a helpful assistant.";
    req.userPrompt = "Translate: {content}";
    req.temperature = 0.3;
    req.maxTokens = 2048;

    Attachment att;
    att.id = "img_001";
    att.mimetype = "image/png";
    att.inlineData = true;
    att.content = "iVBORw0KGgo=";
    req.attachments.push_back(att);

    req.extraParams["top_p"] = "0.9";
    req.extraParams["frequency_penalty"] = "0.5";

    VERIFY(req.model == "gpt-4.1", "model should match");
    VERIFY(req.systemPrompt == "You are a helpful assistant.", "systemPrompt should match");
    VERIFY(req.userPrompt == "Translate: {content}", "userPrompt should match");
    VERIFY(req.temperature == 0.3, "temperature should be 0.3");
    VERIFY(req.maxTokens == 2048, "maxTokens should be 2048");
    VERIFY(req.attachments.size() == 1, "should have 1 attachment");
    VERIFY(req.attachments[0].id == "img_001", "attachment id should match");
    VERIFY(req.extraParams.size() == 2, "should have 2 extra params");
    VERIFY(req.extraParams["top_p"] == "0.9", "top_p should match");
    std::cout << "Test Passed: AIRequest Full Data" << std::endl;
}

void TestAIResponseDefaults() {
    AIResponse resp;
    VERIFY(resp.content.empty(), "default content should be empty");
    VERIFY(resp.model.empty(), "default model should be empty");
    VERIFY(resp.mediaOutputs.empty(), "default mediaOutputs should be empty");
    VERIFY(resp.promptTokens == 0, "default promptTokens should be 0");
    VERIFY(resp.completionTokens == 0, "default completionTokens should be 0");
    std::cout << "Test Passed: AIResponse Defaults" << std::endl;
}

void TestAIResponseFullData() {
    AIResponse resp;
    resp.content = "Hello, world!";
    resp.model = "gpt-4.1";
    resp.promptTokens = 120;
    resp.completionTokens = 45;

    Attachment media;
    media.id = "img_out_001";
    media.mimetype = "image/webp";
    media.file = "blobs/pipeline_output.webp";
    media.size = 1024000;
    resp.mediaOutputs.push_back(media);

    VERIFY(resp.content == "Hello, world!", "content should match");
    VERIFY(resp.model == "gpt-4.1", "model should match");
    VERIFY(resp.promptTokens == 120, "promptTokens should match");
    VERIFY(resp.completionTokens == 45, "completionTokens should match");
    VERIFY(resp.mediaOutputs.size() == 1, "should have 1 media output");
    VERIFY(resp.mediaOutputs[0].id == "img_out_001", "media id should match");
    VERIFY(resp.mediaOutputs[0].mimetype == "image/webp", "media mimetype should match");
    std::cout << "Test Passed: AIResponse Full Data" << std::endl;
}

void TestAttachmentDefaults() {
    Attachment att;
    VERIFY(att.id.empty(), "default id should be empty");
    VERIFY(att.mimetype.empty(), "default mimetype should be empty");
    VERIFY(att.inlineData == false, "default inlineData should be false");
    VERIFY(att.content.empty(), "default content should be empty");
    VERIFY(att.file.empty(), "default file should be empty");
    VERIFY(att.size == 0, "default size should be 0");
    std::cout << "Test Passed: Attachment Defaults" << std::endl;
}

void TestAttachmentInlineData() {
    Attachment att;
    att.id = "img_001";
    att.mimetype = "image/png";
    att.inlineData = true;
    att.content = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==";
    att.size = 86;

    VERIFY(att.inlineData == true, "inline should be true");
    VERIFY(!att.content.empty(), "inline content should not be empty");
    VERIFY(att.file.empty(), "inline should not have file reference");
    VERIFY(att.size == 86, "size should match");
    std::cout << "Test Passed: Attachment Inline Data" << std::endl;
}

void TestAttachmentExternalFile() {
    Attachment att;
    att.id = "img_002";
    att.mimetype = "image/webp";
    att.inlineData = false;
    att.file = "blobs/pipeline_20250530_153042_0.webp";
    att.size = 52428800;

    VERIFY(att.inlineData == false, "external should have inlineData false");
    VERIFY(att.content.empty(), "external should have no content");
    VERIFY(att.file == "blobs/pipeline_20250530_153042_0.webp", "file ref should match");
    VERIFY(att.size == 52428800, "size should match large file size");
    std::cout << "Test Passed: Attachment External File" << std::endl;
}

void TestProviderFactory() {
    // Verify that Create returns nullptr for unknown provider type
    AIProvider *p = AIProvider::Create("nonexistent_provider", "key", "url");
    VERIFY(p == nullptr, "unknown provider type should return nullptr");

    // Verify that Create with empty provider type returns nullptr
    p = AIProvider::Create("", "key", "url");
    VERIFY(p == nullptr, "empty provider type should return nullptr");

    std::cout << "Test Passed: Provider Factory" << std::endl;
}

void TestProviderKnownTypes() {
    // Just verify these known types don't crash when creating (they may need network, so we don't call)
    std::vector<std::string> knownTypes = {"openai", "anthropic", "gemini", "ollama"};
    for (auto &type : knownTypes) {
        AIProvider *p = AIProvider::Create(type, "test_key", "https://api.example.com");
        // These should succeed based on implementation
        // We just verify the pointer is null or non-null without crashing
        if (p) {
            VERIFY(!p->Name().empty(), "provider name should not be empty for type: " + type);
            delete p;
        }
    }
    std::cout << "Test Passed: Provider Known Types" << std::endl;
}

void TestEmptyContentEdgeCases() {
    AIRequest req;
    // Empty fields should be handled gracefully
    VERIFY(req.model.empty(), "empty model");
    req.userPrompt = "";
    VERIFY(req.userPrompt.empty(), "empty user prompt");
    req.systemPrompt = "";
    VERIFY(req.systemPrompt.empty(), "empty system prompt");
    std::cout << "Test Passed: Empty Content Edge Cases" << std::endl;
}

void TestResponseLargeContent() {
    AIResponse resp;
    // Large content string
    std::string large(100000, 'X');
    resp.content = large;
    VERIFY(resp.content.size() == 100000, "large content should be 100KB");
    VERIFY(resp.content[0] == 'X', "first char of large content");
    VERIFY(resp.content[99999] == 'X', "last char of large content");
    std::cout << "Test Passed: Response Large Content" << std::endl;
}

void TestMultipleMediaOutputs() {
    AIResponse resp;
    for (int i = 0; i < 5; i++) {
        Attachment media;
        media.id = "out_" + std::to_string(i);
        media.mimetype = "image/png";
        media.file = "blobs/output_" + std::to_string(i) + ".png";
        media.size = 1000 * (i + 1);
        resp.mediaOutputs.push_back(media);
    }

    VERIFY(resp.mediaOutputs.size() == 5, "should have 5 media outputs");
    VERIFY(resp.mediaOutputs[2].id == "out_2", "third output id");
    VERIFY(resp.mediaOutputs[4].file == "blobs/output_4.png", "fifth output file");
    std::cout << "Test Passed: Multiple Media Outputs" << std::endl;
}

void TestExtraParamsVariety() {
    AIRequest req;
    // Add various extra params
    req.extraParams["temperature"] = "0.8";
    req.extraParams["top_p"] = "0.95";
    req.extraParams["frequency_penalty"] = "0.3";
    req.extraParams["presence_penalty"] = "0.2";
    req.extraParams["stop"] = "[\"\\n\\n\", \"\\n\"]";
    req.extraParams["max_tokens"] = "4096";

    VERIFY(req.extraParams.size() == 6, "should have 6 extra params");
    VERIFY(req.extraParams["temperature"] == "0.8", "temperature");
    VERIFY(req.extraParams["top_p"] == "0.95", "top_p");
    VERIFY(req.extraParams["stop"] == "[\"\\n\\n\", \"\\n\"]", "stop sequences");
    std::cout << "Test Passed: Extra Params Variety" << std::endl;
}

int main() {
    try {
        TestAIRequestDefaults();
        TestAIRequestFullData();
        TestAIResponseDefaults();
        TestAIResponseFullData();
        TestAttachmentDefaults();
        TestAttachmentInlineData();
        TestAttachmentExternalFile();
        TestProviderFactory();
        TestProviderKnownTypes();
        TestEmptyContentEdgeCases();
        TestResponseLargeContent();
        TestMultipleMediaOutputs();
        TestExtraParamsVariety();
        std::cout << "=== ALL PROVIDER TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
