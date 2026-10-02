#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include "../core/streams/ReadableStream.h"
#include "../core/streams/WritableStream.h"
#include "../core/streams/TransformStream.h"
#include "../core/HttpEntities.h"
#include "../adapters/curl/CurlAdapter.h"

using namespace browser_network;

// ====== Testes de ReadableStream ======

void test_readable_stream_basic() {
    std::cout << "test_readable_stream_basic..." << std::endl;
    ReadableStream<ByteBuffer> stream;

    ByteBuffer chunk1 = {1, 2, 3};
    ByteBuffer chunk2 = {4, 5, 6};
    ByteBuffer chunk3 = {7, 8, 9};

    stream.enqueue(chunk1);
    stream.enqueue(chunk2);
    stream.enqueue(chunk3);
    stream.close();

    auto r1 = stream.read();
    assert(r1.has_value());
    assert(r1->size() == 3);
    assert((*r1)[0] == 1);

    auto r2 = stream.read();
    assert(r2.has_value());
    assert((*r2)[0] == 4);

    auto r3 = stream.read();
    assert(r3.has_value());
    assert((*r3)[0] == 7);

    // Fim do stream
    auto r4 = stream.read();
    assert(!r4.has_value());

    std::cout << "test_readable_stream_basic PASSED" << std::endl;
}

void test_readable_stream_backpressure() {
    std::cout << "test_readable_stream_backpressure..." << std::endl;
    ReadableStream<int> stream(2); // high-water mark = 2

    stream.enqueue(1);
    stream.enqueue(2);
    // Fila cheia (2 itens). O próximo enqueue deve bloquear.

    bool producerBlocked = true;
    std::thread producer([&stream, &producerBlocked]() {
        stream.enqueue(3); // Deve bloquear até o consumidor ler
        producerBlocked = false;
    });

    // Aguardar um pouco para garantir que o produtor está bloqueado
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(producerBlocked == true);

    // Consumir um item para liberar espaço
    auto r1 = stream.read();
    assert(r1.has_value());
    assert(*r1 == 1);

    producer.join();
    assert(producerBlocked == false);

    stream.close();

    // Drenar restantes
    auto r2 = stream.read();
    assert(r2.has_value() && *r2 == 2);
    auto r3 = stream.read();
    assert(r3.has_value() && *r3 == 3);
    auto r4 = stream.read();
    assert(!r4.has_value());

    std::cout << "test_readable_stream_backpressure PASSED" << std::endl;
}

void test_readable_stream_error() {
    std::cout << "test_readable_stream_error..." << std::endl;
    ReadableStream<int> stream;

    stream.enqueue(1);
    stream.error("Erro de teste");

    // Leitura após erro deve lançar exceção
    bool threw = false;
    try {
        // Primeiro item na fila pode ser retornado se state check é posterior.
        // Na nossa implementação, se estado é Errored, read() lança exceção.
        stream.read();
    } catch (const std::runtime_error& e) {
        threw = true;
        std::string msg = e.what();
        assert(msg.find("Erro") != std::string::npos);
    }
    assert(threw);

    std::cout << "test_readable_stream_error PASSED" << std::endl;
}

// ====== Testes de WritableStream ======

void test_writable_stream() {
    std::cout << "test_writable_stream..." << std::endl;
    std::vector<int> received;
    bool closed = false;

    WritableStream<int> stream(
        [&received](int chunk) { received.push_back(chunk); },
        [&closed]() { closed = true; }
    );

    stream.write(10);
    stream.write(20);
    stream.write(30);
    stream.close();

    assert(received.size() == 3);
    assert(received[0] == 10);
    assert(received[1] == 20);
    assert(received[2] == 30);
    assert(closed == true);

    // Escrever após fechar deve lançar exceção
    bool threw = false;
    try {
        stream.write(40);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    std::cout << "test_writable_stream PASSED" << std::endl;
}

// ====== Testes de TransformStream ======

void test_transform_stream() {
    std::cout << "test_transform_stream..." << std::endl;

    // Transformação: int -> string (dobrar valor e converter)
    TransformStream<int, std::string> transform([](int val) {
        return std::to_string(val * 2);
    });

    auto writable = transform.getWritable();
    auto readable = transform.getReadable();

    writable->write(5);
    writable->write(10);
    writable->write(15);
    writable->close();

    auto r1 = readable->read();
    assert(r1.has_value());
    assert(*r1 == "10");

    auto r2 = readable->read();
    assert(r2.has_value());
    assert(*r2 == "20");

    auto r3 = readable->read();
    assert(r3.has_value());
    assert(*r3 == "30");

    auto r4 = readable->read();
    assert(!r4.has_value());

    std::cout << "test_transform_stream PASSED" << std::endl;
}

// ====== Testes de Streaming HTTP ======

void test_streaming_http() {
    std::cout << "test_streaming_http..." << std::endl;
    CurlAdapter adapter;

    // httpbin /stream/N retorna N linhas JSON separadas
    HttpRequest req("https://httpbin.org/stream/3", HttpMethod::GET);
    auto future = adapter.sendStreaming(req);
    StreamingResponse response = future.get();

    std::cout << "Streaming Status: " << response.getStatus().code << std::endl;
    assert(response.getStatus().code == 200);

    auto bodyStream = response.getBody();
    std::string fullBody;
    int chunkCount = 0;

    while (true) {
        auto chunk = bodyStream->read();
        if (!chunk.has_value()) break;

        std::string chunkStr(chunk->begin(), chunk->end());
        fullBody += chunkStr;
        chunkCount++;
        std::cout << "  Chunk " << chunkCount << ": " << chunk->size() << " bytes" << std::endl;
    }

    std::cout << "Total chunks: " << chunkCount << ", Total bytes: " << fullBody.size() << std::endl;

    // Verificar que recebemos dados e que contém JSON
    assert(!fullBody.empty());
    assert(fullBody.find("url") != std::string::npos);

    std::cout << "test_streaming_http PASSED" << std::endl;
}

void test_readable_stream_cancel() {
    std::cout << "test_readable_stream_cancel..." << std::endl;
    ReadableStream<int> stream;

    stream.enqueue(1);
    stream.enqueue(2);
    stream.cancel();

    assert(stream.isComplete());
    assert(stream.queueSize() == 0);

    std::cout << "test_readable_stream_cancel PASSED" << std::endl;
}

int main() {
    // Testes de primitivas (Core)
    test_readable_stream_basic();
    test_readable_stream_backpressure();
    test_readable_stream_error();
    test_readable_stream_cancel();
    test_writable_stream();
    test_transform_stream();

    // Teste de integração HTTP Streaming
    test_streaming_http();

    std::cout << "\n=== Todos os testes de WebStreams PASSARAM ===" << std::endl;
    return 0;
}
