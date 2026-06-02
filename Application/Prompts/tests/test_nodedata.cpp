#include "../src/NodeData.h"
#include <cassert>
#include <iostream>
#include <string>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

void TestNodeDefaults() {
    Node n;
    VERIFY(n.title.empty(), "default title should be empty");
    VERIFY(n.content.empty(), "default content should be empty");
    VERIFY(n.mimetype.empty(), "default mimetype should be empty");
    VERIFY(n.attachments.empty(), "default attachments should be empty");
    VERIFY(n.children.empty(), "default children should be empty");
    std::cout << "Test Passed: Node Defaults" << std::endl;
}

void TestNodeWithData() {
    Node n;
    n.title = "SGVsbG8=";        // base64 "Hello"
    n.content = "V29ybGQ=";      // base64 "World"
    n.mimetype = "text/plain";
    VERIFY(n.title == "SGVsbG8=", "title should store base64");
    VERIFY(n.content == "V29ybGQ=", "content should store base64");
    VERIFY(n.mimetype == "text/plain", "mimetype should be text/plain");
    std::cout << "Test Passed: Node With Data" << std::endl;
}

void TestNodeHierarchy() {
    Node parent;
    parent.title = "UGFyZW50";
    parent.mimetype = "text/plain";

    Node child;
    child.title = "Q2hpbGQ=";
    child.mimetype = "text/html";
    parent.children.push_back(child);

    VERIFY(parent.children.size() == 1, "parent should have 1 child");
    VERIFY(parent.children[0].title == "Q2hpbGQ=", "child title should match");
    VERIFY(parent.children[0].mimetype == "text/html", "child mimetype should match");
    std::cout << "Test Passed: Node Hierarchy" << std::endl;
}

void TestAttachment() {
    Attachment att;
    att.id = "img_001";
    att.mimetype = "image/png";
    att.inlineData = true;
    att.content = "iVBORw0KGgo=";
    att.size = 1024;

    VERIFY(att.id == "img_001", "attachment id");
    VERIFY(att.mimetype == "image/png", "attachment mimetype");
    VERIFY(att.inlineData == true, "attachment inline");
    VERIFY(att.content == "iVBORw0KGgo=", "attachment content");
    VERIFY(att.size == 1024, "attachment size");
    std::cout << "Test Passed: Attachment" << std::endl;
}

void TestAttachmentWithFileRef() {
    Attachment att;
    att.id = "img_002";
    att.mimetype = "image/webp";
    att.inlineData = false;
    att.file = "blobs/pipeline_20250530_153042_0.webp";
    att.size = 52428800;

    VERIFY(att.file == "blobs/pipeline_20250530_153042_0.webp", "attachment file ref");
    VERIFY(att.inlineData == false, "external file should not be inline");
    VERIFY(att.content.empty(), "external file should have empty content");
    std::cout << "Test Passed: Attachment With File Reference" << std::endl;
}

void TestSessionData() {
    SessionData session;
    TabData tab1, tab2;
    tab1.name = "General";
    tab1.file = "data/general.json";
    tab2.name = "Code";
    tab2.file = "data/code.json";
    session.tabs.push_back(tab1);
    session.tabs.push_back(tab2);

    VERIFY(session.tabs.size() == 2, "session should have 2 tabs");
    VERIFY(session.tabs[0].name == "General", "first tab name");
    VERIFY(session.tabs[0].file == "data/general.json", "first tab file");
    VERIFY(session.tabs[1].name == "Code", "second tab name");
    std::cout << "Test Passed: Session Data" << std::endl;
}

void TestProviderConfig() {
    ProviderConfig cfg;
    cfg.apiKey = "sk-abc123";
    cfg.baseUrl = "https://api.openai.com/v1";
    cfg.models.push_back("gpt-4");
    cfg.models.push_back("gpt-3.5-turbo");

    VERIFY(cfg.apiKey == "sk-abc123", "provider api key");
    VERIFY(cfg.baseUrl == "https://api.openai.com/v1", "provider base url");
    VERIFY(cfg.models.size() == 2, "provider should have 2 models");
    VERIFY(cfg.models[0] == "gpt-4", "first model");
    std::cout << "Test Passed: Provider Config" << std::endl;
}

void TestPipeline() {
    Pipeline pipe;
    pipe.name = "Translate → Review";
    pipe.mode = "basic";
    pipe.outputMode = "child";
    pipe.multiMedia = "attachments";

    PipelineStep step1;
    step1.name = "Translate";
    step1.type = "ai";
    step1.params["provider"] = "openai";
    step1.params["model"] = "gpt-4.1";

    PipelineStep step2;
    step2.name = "Review";
    step2.type = "ai";
    step2.params["provider"] = "anthropic";

    pipe.steps.push_back(step1);
    pipe.steps.push_back(step2);

    VERIFY(pipe.name == "Translate → Review", "pipeline name");
    VERIFY(pipe.mode == "basic", "pipeline mode");
    VERIFY(pipe.steps.size() == 2, "pipeline should have 2 steps");
    VERIFY(pipe.steps[0].name == "Translate", "first step name");
    VERIFY(pipe.steps[0].params["provider"] == "openai", "first step provider");
    VERIFY(pipe.steps[1].name == "Review", "second step name");
    std::cout << "Test Passed: Pipeline" << std::endl;
}

void TestNodeWithAttachments() {
    Node n;
    n.title = "TXVsdGltb2RhbA==";
    n.mimetype = "text/plain";

    Attachment att1;
    att1.id = "img_001";
    att1.mimetype = "image/png";
    att1.inlineData = true;
    att1.content = "iVBOR...";

    Attachment att2;
    att2.id = "img_002";
    att2.mimetype = "image/webp";
    att2.file = "blobs/pipe_output.webp";

    n.attachments.push_back(att1);
    n.attachments.push_back(att2);

    VERIFY(n.attachments.size() == 2, "node should have 2 attachments");
    VERIFY(n.attachments[0].inlineData == true, "first attachment inline");
    VERIFY(n.attachments[1].inlineData == false, "second attachment external");
    VERIFY(n.attachments[1].file == "blobs/pipe_output.webp", "second attachment file ref");
    std::cout << "Test Passed: Node With Attachments" << std::endl;
}

int main() {
    try {
        TestNodeDefaults();
        TestNodeWithData();
        TestNodeHierarchy();
        TestAttachment();
        TestAttachmentWithFileRef();
        TestSessionData();
        TestProviderConfig();
        TestPipeline();
        TestNodeWithAttachments();
        std::cout << "=== ALL NODE DATA TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
