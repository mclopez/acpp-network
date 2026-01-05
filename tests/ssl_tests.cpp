#include <iostream>
#include <thread>
#include <random>
#include <format>

#include <gtest/gtest.h> // googletest header file  

#include <acpp-network/socket.h>
#include <acpp-network/socket.inl>
#include <detail/common.h>




namespace op_1 {

#include <iostream>
#include <string>

// 1. The "Null" Handler (Stops the recursion)
struct NullLink {
    static void in(std::string&) {}
    static void out(std::string&) {}
};

// 2. The Chain Template
// We use 'Derived' (CRTP-ish) or specific logic blocks
template <typename Self, typename Prior = NullLink, typename Next = NullLink>
struct Handler {
    Prior* prior = nullptr;
    Next* next = nullptr;
    
    void set_next(Next& n) { 
        next = &n; 
        n.prior = (Self*)this;
    }

    void call_next_in(std::string& s) {
        if constexpr (!std::is_same_v<Next, NullLink>) {
            if (next) next->in(s);
        }
    }

    void call_prior_out(std::string& s) {
        if constexpr (!std::is_same_v<Prior, NullLink>) {
            if (prior) prior->out(s);
        }
    }
};

// 3. Concrete Handlers
// We use forward declarations because they point to each other
struct H1;
struct H2;

struct H1 : public Handler<H1, NullLink, H2> {
    void in(std::string& s) {
        std::cout << "H1::in -> " << s << std::endl;
        if (s == "end") {
            call_prior_out(s); // Will do nothing because Prior is NullLink
            return;
        }
        call_next_in(s);
    }

    void out(std::string& s) {
        std::cout << "H1::out -> " << s << std::endl;
    }
};

struct H2 : public Handler<H2, H1, NullLink> {
    void in(std::string& s) {
        std::cout << "H2::in -> " << s << std::endl;
        if (s == "trigger_back") {
            std::string msg = "REVERSE";
            call_prior_out(msg);
            return;
        }
        call_next_in(s);
    }

    void out(std::string& s) {
        std::cout << "H2::out -> " << s << std::endl;
        call_prior_out(s);
    }
};



// 4. Execution
void exec() {
    H1 h1;
    H2 h2;

    // Connect them (Static types, runtime pointers)
    h1.next = &h2;
    h2.prior = &h1; 
 
    std::string msg = "start";
    h1.in(msg);

    std::string trigger = "trigger_back";
    h1.in(trigger);
}

int test() {
    exec();
    return 0;
}

}// namespace op_1



TEST(StreamTests, test_1)
{
op_1::test();
    

}
/*
namespace op_1b {

#include <iostream>
#include <string>

// 1. The "Null" Handler (Stops the recursion)
struct NullLink {
    static void in(std::string&) {}
    static void out(std::string&) {}
};

// 2. The Chain Template
// We use 'Derived' (CRTP-ish) or specific logic blocks
template <typename Self, typename Prior = NullLink, typename Next = NullLink>
struct Handler {
    Prior* prior = nullptr;
    Next* next = nullptr;
    
    void set_next(Next& n) { 
        next = &n; 
        n.prior = (Self*)this;
    }

    void call_next_in(std::string& s) {
        if constexpr (!std::is_same_v<Next, NullLink>) {
            if (next) next->in(s);
        }
    }

    void call_prior_out(std::string& s) {
        if constexpr (!std::is_same_v<Prior, NullLink>) {
            if (prior) prior->out(s);
        }
    }
};

// 3. Concrete Handlers
// We use forward declarations because they point to each other
struct H1;

template <typename Prev, typename Next >
struct H1 : public Handler<H1<Prev, Next>, Prev, Next> {
    void in(std::string& s) {
        std::cout << "H1::in -> " << s << std::endl;
        if (s == "end") {
            call_prior_out(s); // Will do nothing because Prior is NullLink
            return;
        }
        call_next_in(s);
    }

    void out(std::string& s) {
        std::cout << "H1::out -> " << s << std::endl;
    }
};




// 4. Execution
void exec() {
    H1<NullLink, H1<>> h1;
    H1 h2;

    // Connect them (Static types, runtime pointers)
    h1.next = &h2;
    h2.prior = &h1; 
 
    std::string msg = "start";
    h1.in(msg);

    std::string trigger = "trigger_back";
    h1.in(trigger);
}

int test() {
    exec();
    return 0;
}

}// namespace op_1b



TEST(StreamTests, test_1b)
{
op_1::test();
    

}
*/

namespace op_2 {

#include <iostream>
#include <string>
#include <tuple>

// --- The Manager (The "Brain") ---
template <typename... Handlers>
class ChainManager {
    std::tuple<Handlers...> links;

public:
    explicit ChainManager(Handlers... h) : links(h...) {}

    // Start the process at index I
    template <std::size_t I = 0>
    void in(std::string& s) {
        if constexpr (I < sizeof...(Handlers)) {
            // "this" is passed so handlers can call back into the chain
            std::get<I>(links).in(s, *this, std::integral_constant<std::size_t, I>{});
        }
    }

    // Move backward to index I
    template <std::size_t I>
    void out(std::string& s) {
        if constexpr (I < sizeof...(Handlers)) {
            std::get<I>(links).out(s, *this, std::integral_constant<std::size_t, I>{});
        }
    }
};

// --- The Handlers ---
struct H1 {
    template <typename Manager, std::size_t I>
    void in(std::string& s, Manager& m, std::integral_constant<std::size_t, I>) {
        std::cout << "H1::in -> " << s << std::endl;
        if (s == "end") {
            // In the original, this was prior_.out(). 
            // Here, index is 0, so prior doesn't exist.
            return;
        }
        // Move to NEXT: I + 1
        m.template in<I + 1>(s);
    }

    template <typename Manager, std::size_t I>
    void out(std::string& s, Manager& m, std::integral_constant<std::size_t, I>) {
        std::cout << "H1::out -> " << s << std::endl;
        // End of backward chain
    }
};

struct H2 {
    template <typename Manager, std::size_t I>
    void in(std::string& s, Manager& m, std::integral_constant<std::size_t, I>) {
        std::cout << "H2::in -> " << s << std::endl;
        if (s == "reverse") {
            m.template out<I - 1>(s); // Go BACK to H1
            return;
        }
        m.template in<I + 1>(s);
    }

    template <typename Manager, std::size_t I>
    void out(std::string& s, Manager& m, std::integral_constant<std::size_t, I>) {
        std::cout << "H2::out -> " << s << std::endl;
        // Move to PRIOR: I - 1
        if constexpr (I > 0) {
            m.template out<I - 1>(s);
        }
    }
};

int test() {
    ChainManager chain{H1{}, H2{}};
    
    std::string msg = "reverse";
    chain.in(msg); 
    
    return 0;
}


}// namespace op_2


TEST(StreamTests, test_2)
{
    op_2::test();
}

namespace op_2b {

#include <iostream>
#include <string>
#include <tuple>

template <typename T>
using Next = std::integral_constant<typename T::value_type, T::value + 1>;

class Null {};

template<typename Tuple>
 auto get_next(Tuple& ch, size_t i) {

    if (i+1 < std::tuple_size(ch)) {
        return &std::get<i+1>(ch);
    }
    return (Null*) nullptr;
}

// --- The Handlers ---
struct H1 {
    template <typename Chain, std::size_t I>
    void in(const std::string& s, Chain& m, std::integral_constant<std::size_t, I>) {
        std::cout << "H1::in -> " << s << std::endl;
        if (s == "end") {
            // In the original, this was prior_.out(). 
            // Here, index is 0, so prior doesn't exist.
            return;
        }
        // Move to NEXT: I + 1
        //m.template in<I + 1>(s);
        if constexpr (I < std::tuple_size<Chain>()-1) {
            //std::get<1>(handlers).in("hola", handlers, std::integral_constant<std::size_t, 1>{});
            std::get<I+1>(m).in("hola", m, std::integral_constant<std::size_t, I+1>{});


        }
    }

    void in2(const std::string& s) {
        std::cout << "H1::in2 -> " << s << std::endl;
    }


    template <typename Manager, std::size_t I>
    void out(std::string& s, Manager& m, std::integral_constant<std::size_t, I>) {
        std::cout << "H1::out -> " << s << std::endl;
        // End of backward chain
    }
};

/*
struct H2 {
    template <typename Manager, std::size_t I>
    void in(std::string& s, Manager& m, std::integral_constant<std::size_t, I>) {
        std::cout << "H2::in -> " << s << std::endl;
        if (s == "reverse") {
            m.template out<I - 1>(s); // Go BACK to H1
            return;
        }
        m.template in<I + 1>(s);
    }

    template <typename Manager, std::size_t I>
    void out(std::string& s, Manager& m, std::integral_constant<std::size_t, I>) {
        std::cout << "H2::out -> " << s << std::endl;
        // Move to PRIOR: I - 1
        if constexpr (I > 0) {
            m.template out<I - 1>(s);
        }
    }
};
*/


int test() {
    std::tuple handlers{H1{}, H1{}};
    //decltype(handlers)
//<decltype(handlers), std::integral_constant<std::size_t, 1>>
    std::get<1>(handlers).in("hola", handlers, std::integral_constant<std::size_t, 1>{});
    
    //std::string msg = "reverse";
    //chain.in(msg); 
    
    return 0;
}


}// namespace op_b


TEST(StreamTests, test_2b)
{
    op_2b::test();
}


namespace op_3 {

class Null {};

template<typename Next = Null>
struct Handler1 {
    Handler1(){
        if constexpr (!std::is_same_v<Next, Null>) {
            next_.prior_ = (void*)this;
            next_.prior_out_f = &Handler1::static_out;
        }
    }

    static void static_out(void* p, const std::string& s) {
        static_cast<Handler1*>(p)->out(s);
    }
    void in(const std::string& s) {
        std::cout << "Handler1::in" << std::endl;
        if constexpr (!std::is_same_v<Next, Null>) {
            next_.in(s);
        } else {
            //prior_out(s);
            out(s);
        }
    }
    void out(const std::string& s) {
        std::cout << "Handler1::out" << std::endl;
        prior_out(s);
    }
    void prior_out(const std::string& s) {
        if (prior_out_f && prior_) 
            prior_out_f(prior_, s);
    }
    Next next_;
    void(*prior_out_f)(void*, const std::string&);
    void* prior_ = nullptr;
};


void test() {
    Handler1<Handler1<Handler1<>>> h;
    h.in("hola");
}

}// namespace op_3

TEST(StreamTests, test_3)
{
    op_3::test();
}


namespace op_3b {

class Null {
public:
    template <typename Callback> 
    void init(Callback&& callback) {
        std::cout << "Null::init" << std::endl;
        callback();
    }

    template <typename Prev> 
    void in2(Prev& prev, const std::string& s) {
        std::cout << "Null::in2" << std::endl;
        //prev.out2(s);
    }
    template <typename Prev> 
    void out2(Prev& prev, const std::string& s) {
        std::cout << "Null::out2" << std::endl;
        prev.out2(prev, s);
    }

};

template<typename Next = Null>
struct Handler1 {

    Handler1(){
        if constexpr (!std::is_same_v<Next, Null>) {
            next_.prior_ = (void*)this;
            next_.prior_out_f = &Handler1::static_out;
        }
    }

    static void static_out(void* p, const std::string& s) {
        static_cast<Handler1*>(p)->out(s);
    }
    
    template <typename Callback> 
    void init(Callback&& cb) {
        std::cout << "Handler1::init" << std::endl;
        auto callback = [cb=std::move(cb)]{
            std::cout << "Handler1::init callback" << std::endl;
            cb();
        };
        next_.init(std::move(callback));
    }

    void in(const std::string& s) {
        std::cout << "Handler1::in" << std::endl;
        if constexpr (!std::is_same_v<Next, Null>) {
            next_.in(s);
        } else {
            //prior_out(s);
            out(s);
        }
    }

    void out(const std::string& s) {
        std::cout << "Handler1::out" << std::endl;
        prior_out(s);
    }

    template <typename Prev> 
    void in2(Prev& prev, const std::string& s) {
        std::cout << "Handler1::init" << std::endl;
        // auto callback = [cb=std::move(cb)]{
        //     std::cout << "Handler1::init callback" << std::endl;
        //     cb();
        // };
        next_.in2(*this, s);
        prev.out2(prev, s);
    }

    template <typename Prev> 
    void out2(Prev& prev, const std::string& s) {
        std::cout << "Handler1::out2" << std::endl;
        prev.out2(prev, s);
    }


    void prior_out(const std::string& s) {
        if (prior_out_f && prior_) 
            prior_out_f(prior_, s);
    }
    Next next_;
    void(*prior_out_f)(void*, const std::string&);
    void* prior_ = nullptr;
};


void test() {
    Handler1<Handler1<Handler1<>>> h;
    auto callback = [](){
        std::cout << "test callback" << std::endl;
    };
    h.init(callback);
    //h.in("hola");
    //Null p;
    //h.in2(p, "hola2");
}

}// namespace op_3b

TEST(StreamTests, test_3b)
{
    op_3b::test();
}


namespace op_4 {

struct Null;

struct Prior {

    void* prior_ = nullptr;
    void(*prior_out_f)(void*, const std::string&);
    
    Prior():prior_ (nullptr), prior_out_f(nullptr){}

    template<typename Handler> 
    Prior(Handler& h) {
        if constexpr (!std::is_same_v<Handler, Null>) {
            prior_ = (void*)&h;
            prior_out_f = &Handler::static_out;
        } else {
            prior_ = nullptr;
            prior_out_f = nullptr;
        }
    }
    void out(const std::string& s) {
        if (prior_out_f && prior_) 
            prior_out_f(prior_, s);
    }
};

struct Null {
    Prior prior_;
};


template<typename Next = Null>
struct Handler {
    Handler(){
        next_.prior_ = Prior(*this);
    }
    using next_type = Next;

    static void static_out(void* p, const std::string& s) {
        static_cast<Handler*>(p)->out(s);
    }
    void in(const std::string& s) {
        std::cout << "Handler1::in" << std::endl;
        if constexpr (!std::is_same_v<Next, Null>) {
            next_.in(s);
        } else {
            //prior_out(s);
            out(s);
        }
    }
    void out(const std::string& s) {
        std::cout << "Handler1::out" << std::endl;
        prior_.out(s);
    }

    Next next_;
    Prior prior_;
};


void test() {
    Handler<Handler<Handler<>>> h;
    h.in("hola");
}

}// namespace op_4

TEST(StreamTests, test_4)
{
    op_4::test();
}



namespace op_5 {

struct Null;

struct Prior {

    void* prior_ = nullptr;
    void(*prior_out_f)(void*, const std::string&);
    
    Prior():prior_ (nullptr), prior_out_f(nullptr){}

    template<typename Handler> 
    Prior(Handler& h) {
        if constexpr (!std::is_same_v<Handler, Null>) {
            prior_ = (void*)&h;
            prior_out_f = &Handler::static_out;
        } else {
            prior_ = nullptr;
            prior_out_f = nullptr;
        }
    }
    void out(const std::string& s) {
        if (prior_out_f && prior_) 
            prior_out_f(prior_, s);
    }
};

struct Null {
    Prior prior_;
};


template<typename Next = Null>
struct Handler {

    Handler(){
        next_.prior_ = Prior(*this);
    }

    using next_type = Next;
    
    //using chain = Next<Handler>

    //using chain = std::tuple<Handler, next_type> = template(typename This, typename T2) 

    // std::tuple<This, T2> do_nothing(This& t, T2& next) {

    //     return std::tuple<This*, T2*>{&t, t2};
    // }

    static void static_out(void* p, const std::string& s) {
        static_cast<Handler*>(p)->out(s);
    }

    void in(const std::string& s) {
        std::cout << "Handler1::in " << s << std::endl;
        if constexpr (!std::is_same_v<Next, Null>) {
            next_.in(s);
        } else {
            //prior_out(s);
            out(s);
        }
    }
    void out(const std::string& s) {
        std::cout << "Handler1::out " << s << std::endl;
        prior_.out(s);
    }

    Next next_;
    Prior prior_;
};


void test() {
    Handler<Handler<Handler<>>> h;
    h.in("hola");
}

}// namespace op_5

TEST(StreamTests, test_5)
{
    op_5::test();
}

/*
namespace op_6 {
#include <iostream>
#include <string>

// Forward declarations
template<typename Prev, typename Next>
class Layer;

// Sentinel types for boundaries
struct NoLayer {};

// Base layer template with prev and next access
template<typename Prev = NoLayer, typename Next = NoLayer>
class Layer {
protected:
    Prev* prev;
    Next* next;

public:
    Layer() : prev(nullptr), next(nullptr) {}

    void setPrev(Prev* p) { prev = p; }
    void setNext(Next* n) { next = n; }

    Prev* getPrev() { return prev; }
    Next* getNext() { return next; }
};

// Specialized layer types
template<typename Prev, typename Next>
class TransportLayer : public Layer<Prev, Next> {
public:
    void send(const std::string& data) {
        std::cout << "[Transport] Sending: " << data << std::endl;
        
        // Can call next layer if it exists
        if constexpr (!std::is_same_v<Next, NoLayer>) {
            if (this->next) {
                this->next->processOutgoing(data);
            }
        }
    }

    void receive(const std::string& data) {
        std::cout << "[Transport] Received: " << data << std::endl;
        
        // Can call previous layer if it exists
        if constexpr (!std::is_same_v<Prev, NoLayer>) {
            if (this->prev) {
                this->prev->processIncoming(data);
            }
        }
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Transport] Processing outgoing: " << data << std::endl;
        send(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Transport] Processing incoming: " << data << std::endl;
        receive(data);
    }
};

template<typename Prev, typename Next>
class NetworkLayer : public Layer<Prev, Next> {
public:
    void route(const std::string& data) {
        std::cout << "[Network] Routing: " << data << std::endl;
        
        if constexpr (!std::is_same_v<Next, NoLayer>) {
            if (this->next) {
                this->next->processOutgoing(data);
            }
        }
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Network] Adding IP header to: " << data << std::endl;
        route(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Network] Removing IP header from: " << data << std::endl;
        
        if constexpr (!std::is_same_v<Prev, NoLayer>) {
            if (this->prev) {
                this->prev->processIncoming(data);
            }
        }
    }
};

template<typename Prev, typename Next>
class DataLinkLayer : public Layer<Prev, Next> {
public:
    void transmit(const std::string& data) {
        std::cout << "[DataLink] Transmitting: " << data << std::endl;
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[DataLink] Adding MAC header to: " << data << std::endl;
        transmit(data);
        
        // Simulate receiving on the other end
        std::cout << "\n--- Simulating reception ---\n" << std::endl;
        processIncoming(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[DataLink] Removing MAC header from: " << data << std::endl;
        
        if constexpr (!std::is_same_v<Prev, NoLayer>) {
            if (this->prev) {
                this->prev->processIncoming(data);
            }
        }
    }
};

// Helper function to link layers
template<typename Layer1, typename Layer2>
void linkLayers(Layer1& layer1, Layer2& layer2) {
    layer1.setNext(&layer2);
    layer2.setPrev(&layer1);
}

int test() {
    // Define the layer stack with explicit type relationships
    using DLL = DataLinkLayer<NoLayer, NoLayer>;
    using NL = NetworkLayer<DLL, NoLayer>;
    using TL = TransportLayer<NL, NoLayer>;

    // Create instances
    DLL dataLink;
    NL network;
    TL transport;

    // Link them together
    linkLayers(dataLink, network);
    linkLayers(network, transport);

    std::cout << "=== Sending data down the stack ===\n" << std::endl;
    transport.send("Hello, World!");

    std::cout << "\n=== Accessing adjacent layers ===\n" << std::endl;
    
    // Transport can access network layer
    auto* netLayer = transport.getPrev();
    if (netLayer) {
        std::cout << "Transport accessing Network layer..." << std::endl;
        netLayer->route("Direct call from transport");
    }

    // Network can access both neighbors
    auto* transportLayer = network.getNext();
    auto* dataLinkLayer = network.getPrev();
    
    if (transportLayer) {
        std::cout << "\nNetwork accessing Transport layer..." << std::endl;
        transportLayer->send("Direct call from network");
    }

    return 0;
}

}//namespace op_6

TEST(StreamTests, test_56)
{
    op_6::test();
}
*/
namespace op_7 {
#include <iostream>
#include <string>

// Sentinel type for boundaries
struct NoLayer {
    void processOutgoing(const std::string&) {}
    void processIncoming(const std::string&) {}
};

// Base layer using CRTP
template<typename Derived, typename Prev = NoLayer, typename Next = NoLayer>
class Layer {
protected:
    Prev* prev;
    Next* next;

public:
    Layer() : prev(nullptr), next(nullptr) {}

    void setPrev(Prev* p) { prev = p; }
    void setNext(Next* n) { next = n; }

    Prev* getPrev() { return prev; }
    Next* getNext() { return next; }

    Derived& self() { return static_cast<Derived&>(*this); }
};

// Transport Layer
template<typename Prev = NoLayer, typename Next = NoLayer>
class TransportLayer : public Layer<TransportLayer<Prev, Next>, Prev, Next> {
public:
    void send(const std::string& data) {
        std::cout << "[Transport] Sending: " << data << std::endl;
        
        if constexpr (!std::is_same_v<Next, NoLayer>) {
            if (this->next) {
                this->next->processOutgoing(data);
            }
        }
    }

    void receive(const std::string& data) {
        std::cout << "[Transport] Received: " << data << std::endl;
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Transport] Processing outgoing: " << data << std::endl;
        send(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Transport] Processing incoming: " << data << std::endl;
        receive(data);
    }
};

// Network Layer
template<typename Prev = NoLayer, typename Next = NoLayer>
class NetworkLayer : public Layer<NetworkLayer<Prev, Next>, Prev, Next> {
public:
    void route(const std::string& data) {
        std::cout << "[Network] Routing: " << data << std::endl;
        
        if constexpr (!std::is_same_v<Next, NoLayer>) {
            if (this->next) {
                this->next->processOutgoing(data);
            }
        }
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Network] Adding IP header to: " << data << std::endl;
        route(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Network] Removing IP header from: " << data << std::endl;
        
        if constexpr (!std::is_same_v<Prev, NoLayer>) {
            if (this->prev) {
                this->prev->processIncoming(data);
            }
        }
    }
};

// DataLink Layer
template<typename Prev = NoLayer, typename Next = NoLayer>
class DataLinkLayer : public Layer<DataLinkLayer<Prev, Next>, Prev, Next> {
public:
    void transmit(const std::string& data) {
        std::cout << "[DataLink] Transmitting: " << data << std::endl;
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[DataLink] Adding MAC header to: " << data << std::endl;
        transmit(data);
        
        // Simulate receiving on the other end
        std::cout << "\n--- Simulating reception ---\n" << std::endl;
        processIncoming(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[DataLink] Removing MAC header from: " << data << std::endl;
        
        if constexpr (!std::is_same_v<Prev, NoLayer>) {
            if (this->prev) {
                this->prev->processIncoming(data);
            }
        }
    }
};

int test() {
    // Create instances - each layer is independent
    DataLinkLayer<> dataLink;
    NetworkLayer<> network;
    TransportLayer<> transport;

    // Manually link them by casting pointers
    // This is the tradeoff: type safety at link time vs compile time
    dataLink.setNext(reinterpret_cast<NoLayer*>(&network));
    network.setPrev(reinterpret_cast<NoLayer*>(&dataLink));
    network.setNext(reinterpret_cast<NoLayer*>(&transport));
    transport.setPrev(reinterpret_cast<NoLayer*>(&network));

    std::cout << "=== Sending data down the stack ===\n" << std::endl;
    transport.send("Hello, World!");

    std::cout << "\n=== Accessing adjacent layers directly ===\n" << std::endl;
    
    // Access neighbors with proper casting
    auto* netLayer = reinterpret_cast<NetworkLayer<>*>(transport.getPrev());
    if (netLayer) {
        std::cout << "Transport accessing Network layer..." << std::endl;
        netLayer->route("Direct call from transport");
    }

    auto* dataLinkLayer = reinterpret_cast<DataLinkLayer<>*>(network.getPrev());
    if (dataLinkLayer) {
        std::cout << "\nNetwork accessing DataLink layer..." << std::endl;
        dataLinkLayer->transmit("Direct call from network");
    }

    auto* transportLayer = reinterpret_cast<TransportLayer<>*>(network.getNext());
    if (transportLayer) {
        std::cout << "\nNetwork accessing Transport layer..." << std::endl;
        transportLayer->send("Direct call from network to transport");
    }

    return 0;
}
} //namespace op_7


TEST(StreamTests, test_7)
{
    op_7::test();
}


namespace op_8 {
#include <iostream>
#include <string>

// Sentinel type for boundaries
struct NoLayer {
    void processOutgoing(const std::string&) {}
    void processIncoming(const std::string&) {}
};

// Base layer using CRTP
template<typename Derived, typename Prev = NoLayer, typename Next = NoLayer>
class Layer {
protected:
    Prev* prev;
    Next* next;

public:
    Layer() : prev(nullptr), next(nullptr) {}

    void setPrev(Prev* p) { prev = p; }
    void setNext(Next* n) { next = n; }

    Prev* getPrev() { return prev; }
    Next* getNext() { return next; }

    Derived& self() { return static_cast<Derived&>(*this); }
};

// Forward declaration
template<typename Prev, typename Next>
class NetworkLayer;

// Define the type where NetworkLayer is connected to itself
using SelfConnectedNetwork = NetworkLayer<NetworkLayer<NoLayer, NoLayer>, NetworkLayer<NoLayer, NoLayer>>;

// Network Layer implementation
template<typename Prev = NoLayer, typename Next = NoLayer>
class NetworkLayer : public Layer<NetworkLayer<Prev, Next>, Prev, Next> {
public:
    void route(const std::string& data) {
        std::cout << "[Network] Routing: " << data << std::endl;
        
        if constexpr (!std::is_same_v<Next, NoLayer>) {
            if (this->next) {
                this->next->processOutgoing(data);
            }
        }
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Network] Processing outgoing: " << data << std::endl;
        std::cout << "[Network] Adding IP header to: " << data << std::endl;
        route(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Network] Processing incoming: " << data << std::endl;
        std::cout << "[Network] Removing IP header from: " << data << std::endl;
        
        if constexpr (!std::is_same_v<Prev, NoLayer>) {
            if (this->prev) {
                this->prev->processIncoming(data);
            }
        }
    }

    void send(const std::string& data) {
        std::cout << "[Network] Sending: " << data << std::endl;
        processOutgoing(data);
    }
};

int test() {
    // Create three network layer instances
    NetworkLayer<NoLayer, NoLayer> net1;
    NetworkLayer<NoLayer, NoLayer> net2;
    NetworkLayer<NoLayer, NoLayer> net3;

    // Connect them: net1 <-> net2 <-> net3
    net1.setNext(reinterpret_cast<NoLayer*>(&net2));
    net2.setPrev(reinterpret_cast<NoLayer*>(&net1));
    net2.setNext(reinterpret_cast<NoLayer*>(&net3));
    net3.setPrev(reinterpret_cast<NoLayer*>(&net2));

    std::cout << "=== Sending from net1 through the chain ===\n" << std::endl;
    net1.send("Hello from net1");

    std::cout << "\n=== Net2 accessing its neighbors ===\n" << std::endl;
    
    auto* prevLayer = reinterpret_cast<NetworkLayer<>*>(net2.getPrev());
    if (prevLayer) {
        std::cout << "Net2 accessing previous (net1)..." << std::endl;
        prevLayer->route("Message to net1");
    }

    auto* nextLayer = reinterpret_cast<NetworkLayer<>*>(net2.getNext());
    if (nextLayer) {
        std::cout << "\nNet2 accessing next (net3)..." << std::endl;
        nextLayer->route("Message to net3");
    }

    std::cout << "\n=== Net3 accessing net2 ===\n" << std::endl;
    auto* prevFromNet3 = reinterpret_cast<NetworkLayer<>*>(net3.getPrev());
    if (prevFromNet3) {
        std::cout << "Net3 accessing previous (net2)..." << std::endl;
        prevFromNet3->send("Message from net3 to net2");
    }

    return 0;
}
}//namespace op_8

TEST(StreamTests, test_8)
{
    op_8::test();
}



namespace op_9 {

    #include <iostream>
#include <string>
#include <utility>

// Base interface that all layers must implement
template<typename Derived>
class LayerInterface {
public:
    void processOutgoing(const std::string& data) {
        static_cast<Derived*>(this)->processOutgoing(data);
    }
    
    void processIncoming(const std::string& data) {
        static_cast<Derived*>(this)->processIncoming(data);
    }
};

// Layer that contains the next layer and a pointer to previous
template<typename Derived, typename NextLayer>
class Layer : public LayerInterface<Derived> {
protected:
    NextLayer next;
    void* prev; // Type-erased pointer to previous layer
    
public:
    Layer() : prev(nullptr) {}
    
    // Initialize with next layer
    template<typename... Args>
    Layer(Args&&... args) : next(std::forward<Args>(args)...), prev(nullptr) {}
    
    NextLayer& getNext() { return next; }
    const NextLayer& getNext() const { return next; }
    
    template<typename PrevType>
    PrevType& getPrev() { 
        return *static_cast<PrevType*>(prev); 
    }
    
    template<typename PrevType>
    void setPrev(PrevType* p) { 
        prev = static_cast<void*>(p); 
    }
};

// Terminal layer (no next layer)
template<typename Derived>
class TerminalLayer : public LayerInterface<Derived> {
protected:
    void* prev;
    
public:
    TerminalLayer() : prev(nullptr) {}
    
    template<typename PrevType>
    PrevType& getPrev() { 
        return *static_cast<PrevType*>(prev); 
    }
    
    template<typename PrevType>
    void setPrev(PrevType* p) { 
        prev = static_cast<void*>(p); 
    }
};

// Forward declarations
template<typename Next> class TransportLayer;
template<typename Next> class NetworkLayer;
class DataLinkLayer;

// Transport Layer
template<typename Next>
class TransportLayer : public Layer<TransportLayer<Next>, Next> {
public:
    using Layer<TransportLayer<Next>, Next>::Layer;
    
    void send(const std::string& data) {
        std::cout << "[Transport] Sending: " << data << std::endl;
        processOutgoing(data);
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Transport] Processing outgoing: " << data << std::endl;
        this->next.processOutgoing(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Transport] Processing incoming: " << data << std::endl;
        std::cout << "[Transport] Received: " << data << std::endl;
    }
};

// Network Layer
template<typename Next>
class NetworkLayer : public Layer<NetworkLayer<Next>, Next> {
public:
    using Layer<NetworkLayer<Next>, Next>::Layer;
    
    void route(const std::string& data) {
        std::cout << "[Network] Routing: " << data << std::endl;
        this->next.processOutgoing(data);
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Network] Adding IP header to: " << data << std::endl;
        route(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Network] Removing IP header from: " << data << std::endl;
        // Call previous layer
        this->template getPrev<TransportLayer<NetworkLayer<DataLinkLayer>>>().processIncoming(data);
    }
};

// DataLink Layer (terminal)
class DataLinkLayer : public TerminalLayer<DataLinkLayer> {
public:
    void transmit(const std::string& data) {
        std::cout << "[DataLink] Transmitting: " << data << std::endl;
        std::cout << "\n--- Simulating reception ---\n" << std::endl;
        processIncoming(data);
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[DataLink] Adding MAC header to: " << data << std::endl;
        transmit(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[DataLink] Removing MAC header from: " << data << std::endl;
        // Call previous layer
        this->getPrev<NetworkLayer<DataLinkLayer>>().processIncoming(data);
    }
};

// Type alias for the complete stack
using LayerStack = TransportLayer<NetworkLayer<DataLinkLayer>>;

int test() {
    // Create the entire stack as one object
    // Each layer contains the next layer as a member
    LayerStack stack;
    
    // Set up backward pointers
    stack.getNext().setPrev(&stack);
    stack.getNext().getNext().setPrev(&stack.getNext());

    std::cout << "=== Sending data down the stack ===\n" << std::endl;
    stack.send("Hello, World!");

    std::cout << "\n=== Accessing layers directly ===\n" << std::endl;
    
    // Access network layer from transport
    auto& networkLayer = stack.getNext();
    std::cout << "Transport accessing Network layer..." << std::endl;
    networkLayer.route("Direct call from transport");

    // Access datalink layer from network
    auto& dataLinkLayer = networkLayer.getNext();
    std::cout << "\nNetwork accessing DataLink layer..." << std::endl;
    dataLinkLayer.transmit("Direct call from network");

    // Access transport from network (using getPrev)
    std::cout << "\nNetwork accessing Transport layer..." << std::endl;
    networkLayer.getPrev<TransportLayer<NetworkLayer<DataLinkLayer>>>().send("Call from network to transport");

    return 0;
}
}// namespace op_9


TEST(StreamTests, test_9)
{
    op_9::test();
}

namespace op_10{
#include <iostream>
#include <string>
#include <utility>

// Layer that contains the next layer and a pointer to previous
template<typename Derived, typename NextLayer>
class Layer {
protected:
    NextLayer next;
    void* prev; // Type-erased pointer to previous layer
    
public:
    Layer() : prev(nullptr) {}
    
    // Initialize with next layer
    template<typename... Args>
    Layer(Args&&... args) : next(std::forward<Args>(args)...), prev(nullptr) {}
    
    NextLayer& getNext() { return next; }
    const NextLayer& getNext() const { return next; }
    
    template<typename PrevType>
    PrevType& getPrev() { 
        return *static_cast<PrevType*>(prev); 
    }
    
    template<typename PrevType>
    void setPrev(PrevType* p) { 
        prev = static_cast<void*>(p); 
    }
    
    Derived& self() { return static_cast<Derived&>(*this); }
};

// Terminal layer (no next layer)
template<typename Derived>
class TerminalLayer {
protected:
    void* prev;
    
public:
    TerminalLayer() : prev(nullptr) {}
    
    template<typename PrevType>
    PrevType& getPrev() { 
        return *static_cast<PrevType*>(prev); 
    }
    
    template<typename PrevType>
    void setPrev(PrevType* p) { 
        prev = static_cast<void*>(p); 
    }
    
    Derived& self() { return static_cast<Derived&>(*this); }
};

// Transport Layer - generic, works with any Next type
template<typename Next>
class TransportLayer : public Layer<TransportLayer<Next>, Next> {
public:
    using Layer<TransportLayer<Next>, Next>::Layer;
    
    void send(const std::string& data) {
        std::cout << "[Transport] Sending: " << data << std::endl;
        processOutgoing(data);
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Transport] Processing outgoing: " << data << std::endl;
        this->next.processOutgoing(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Transport] Processing incoming: " << data << std::endl;
        std::cout << "[Transport] Received: " << data << std::endl;
    }
};

// Network Layer - generic, works with any Next type
template<typename Next>
class NetworkLayer : public Layer<NetworkLayer<Next>, Next> {
public:
    using Layer<NetworkLayer<Next>, Next>::Layer;
    
    void route(const std::string& data) {
        std::cout << "[Network] Routing: " << data << std::endl;
        this->next.processOutgoing(data);
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Network] Adding IP header to: " << data << std::endl;
        route(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Network] Removing IP header from: " << data << std::endl;
    }
};

// DataLink Layer - terminal layer
class DataLinkLayer : public TerminalLayer<DataLinkLayer> {
public:
    void transmit(const std::string& data) {
        std::cout << "[DataLink] Transmitting: " << data << std::endl;
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[DataLink] Adding MAC header to: " << data << std::endl;
        transmit(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[DataLink] Removing MAC header from: " << data << std::endl;
    }
};

// Physical Layer - another terminal layer option
class PhysicalLayer : public TerminalLayer<PhysicalLayer> {
public:
    void sendBits(const std::string& data) {
        std::cout << "[Physical] Sending bits: " << data << std::endl;
    }

    void processOutgoing(const std::string& data) {
        std::cout << "[Physical] Converting to electrical signals: " << data << std::endl;
        sendBits(data);
    }

    void processIncoming(const std::string& data) {
        std::cout << "[Physical] Converting from electrical signals: " << data << std::endl;
    }
};

// Helper function to set up backward links
template<typename Stack>
void setupBackwardLinks(Stack& stack) {
    auto& layer1 = stack;
    auto& layer2 = layer1.getNext();
    layer2.setPrev(&layer1);
    
    if constexpr (requires { layer2.getNext(); }) {
        auto& layer3 = layer2.getNext();
        layer3.setPrev(&layer2);
    }
}

int test() {
    std::cout << "=== Stack 1: Transport -> Network -> DataLink ===\n" << std::endl;
    
    // First stack configuration
    using Stack1 = TransportLayer<NetworkLayer<DataLinkLayer>>;
    Stack1 stack1;
    setupBackwardLinks(stack1);
    
    stack1.send("Message on Stack1");

    std::cout << "\n=== Stack 2: Transport -> Network -> Physical ===\n" << std::endl;
    
    // Second stack configuration - different bottom layer!
    using Stack2 = TransportLayer<NetworkLayer<PhysicalLayer>>;
    Stack2 stack2;
    setupBackwardLinks(stack2);
    
    stack2.send("Message on Stack2");

    std::cout << "\n=== Stack 3: Network -> DataLink (no Transport) ===\n" << std::endl;
    
    // Third stack configuration - different layers entirely!
    using Stack3 = NetworkLayer<DataLinkLayer>;
    Stack3 stack3;
    setupBackwardLinks(stack3);
    
    stack3.route("Direct routing on Stack3");

    std::cout << "\n=== Stack 4: Transport -> Physical (skip Network) ===\n" << std::endl;
    
    // Fourth stack - skip middle layer
    using Stack4 = TransportLayer<PhysicalLayer>;
    Stack4 stack4;
    setupBackwardLinks(stack4);
    
    stack4.send("Direct to physical on Stack4");

    std::cout << "\n=== Accessing adjacent layers ===\n" << std::endl;
    
    // Access layers in stack1
    auto& net1 = stack1.getNext();
    std::cout << "Stack1 - Transport accessing Network:" << std::endl;
    net1.route("Direct call");
    
    auto& dl1 = net1.getNext();
    std::cout << "\nStack1 - Network accessing DataLink:" << std::endl;
    dl1.transmit("Direct call");

    return 0;
}
}//namespace op_10

TEST(StreamTests, test_10)
{
    op_10::test();
}


namespace op_11 {

template<typename Tuple, typename T>
struct AppendToTuple;

template<typename... Ts, typename T>
struct AppendToTuple<std::tuple<Ts...>, T> {
    using type = std::tuple<Ts..., T>;
};

// Helper alias
template<typename Tuple, typename T>
using AppendToTuple_t = typename AppendToTuple<Tuple, T>::type;


template<typename T, typename Tuple>
struct PrependToTuple;

template<typename T, typename... Ts>
struct PrependToTuple<T, std::tuple<Ts...>> {
    using type = std::tuple<T, Ts...>;
};

template<typename T, typename Tuple>
using PrependToTuple_t = typename PrependToTuple<T, Tuple>::type;



struct Null{
    enum {it = 0,};  
    using chain_type = std::tuple<Null*>;
    chain_type chain = {this};

    template<typename Chain>
    Null(Chain& ch) {
        std::get<it>(ch) = this;
    }

    void in() {
        std::cout << "Null::in: "<< it << std::endl;
    }

    template<typename Chain=chain_type> 
    void in2(Chain& chain) {
        std::cout << "Null::in2: "<< it << std::endl;
        out2(chain);
    }

    template<typename Chain=chain_type>
    auto get_prev(Chain& chain) {
        if constexpr (it + 1 < std::tuple_size<Chain>()) {
            return std::get<it + 1>(chain);
        } else {
            return (Null*) nullptr;
        }
    }

    template<typename Chain=chain_type> 
    void out2(Chain& chain) {
        std::cout << "Null::out2: "<< it << std::endl;

        auto prev = get_prev(chain);
        if (prev != nullptr)
            prev->out2(chain);

    }
};

template <typename Next=Null>
struct Handler {
    enum {it = Next::it+1,};  
    using chain_type = AppendToTuple_t<typename Next::chain_type, Handler* >;
    
    chain_type chain; //just for first Handler
    Handler():next_(chain) {
        std::get<it>(chain) = this;
    }

    template<typename Chain>
    Handler(Chain& ch):next_(ch) {
        std::get<it>(ch) = this;
    }

    void in() {
        std::cout << "Handler::in: "<< it << std::endl;
        next_.in();
    }

    template<typename Chain=chain_type> 
    void in2(Chain& chain) {
        std::cout << "Handler::in2: "<< it << std::endl;
        next_.in2(chain);
    }
    void in2() {
        std::cout << "Handler::in2: "<< it << std::endl;
        next_.in2(chain);
    }

    template<typename Chain=chain_type> 
    void out2(Chain& chain) {
        std::cout << "Handler::out2: "<< it << std::endl;
        auto prev = get_prev(chain);
        if (prev != nullptr)
            prev->out2(chain);
    }

    template<typename Chain=chain_type>
    auto get_prev(Chain& chain) {
        if constexpr (it + 1 < std::tuple_size<Chain>()) {
            return std::get<it + 1>(chain);
        } else {
            return (Null*) nullptr;
        }
    }

    Next next_;
};

void test() {
    using H = Handler<Handler<Handler< >>>;
    //H::chain_type ch;
    H h;
    std::cout << std::tuple_size<H::chain_type>() << std::endl;

    h.in2();
}

}//namespace op_11

TEST(StreamTests, test_11)
{
    op_11::test();
}







