#include <iostream>
#include <thread>
#include <atomic>


template<typename T,size_t N>

struct SPSCQueue{
    
    bool push(const T& val)
    {
        size_t tail = tail_.load(std::memory_order_relaxed);  // local = tail
        size_t head = head_.load(std::memory_order_acquire);  // local = head
        
        if((tail+1)%N == head) return false;
        
        data_[tail] = val;
        tail_.store((tail+1)%N, std::memory_order_release);   // member = tail_
        return true;
    }
    
    bool pop( T& val)
    {
        size_t tail = tail_.load(std::memory_order_acquire);
        size_t head = head_.load(std::memory_order_relaxed);
        
        if(tail == head) return false;
        
        val = data_[head] ;
        head_.store((head+1)%N, std::memory_order_release);    // member = tail_
        return true;
        
    }

private:
    alignas(64) T data_[N];
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    
};

int main()
{
    SPSCQueue<int, 1024> queue;
    
    std::thread producer([&] {
        for(int i=0; i<=99;i++)
        {
            while(!queue.push(i));
        }
    });
    
    std::thread consumer([&]
    {
        int val;
       for(int i=0; i<=99;i++)
       {
           while(!queue.pop(val));
           std::cout<<val<<'\n';
       }
    });
    producer.join();
    consumer.join();
    
    return 0;
}