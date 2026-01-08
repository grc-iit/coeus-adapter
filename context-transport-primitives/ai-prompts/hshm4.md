# Lightbeam

This is a library for transfering pieces of data over a network. For now, only ZeroMQ. Take inspiration from and then remove the existing Send/Recv functions. Implement the api below. Then write a unit test for it. 

Messages will be sent in two parts:
1. The Metadata payload
2. The Data payloads

## Basic Metadata Class
Metadata contains the shape of the message. I.e., the bulk transfer objects to transmit.
```cpp
class LbmMeta {
 public:
  std::vector<Bulk> bulks;
}
```

Other, more complex, Metadata classes can inherit from this base class.

## Bulk class

Update the existing bulk class to store a FullPtr<char> instead of a char* for data. No other changes needed.

## ZeroMQ

### Client
Main functions:
1. Expose. Like it is now, but update to use FullPtr instead of char * for data. 
2. template<typename MetaT> Send(MetaT &Meta): Serialize the MetaT using cereal::BinaryOutputArchive. Send over network. Then send each individual bulk over network. Use only non-blocking primitives. Use ZMQ_SNDMORE for making the multi-part message.

### Server
1. Expose. Same as Client.
2. template<typename MetaT> RecvMetadata(MetaT &meta): Deserialize the MetaT using cereal. This will not allocate Bulks on the server. The user is responsible for allocating the bulks manually after this function.
3. template<typename MetaT> RecvBulks(MetaT &meta): Receive each bulk stored in the meta.

This is split into two functions because we want to give users the chance to allocate the data for their bulks. 
Lightbeam is not responsible for freeing the data pointed to by bulks.

## SendIn

1. ``ar << task``. Bulks stored in a vector. 
2. Send(ar)

## LoadIn

1.

## SendOut

## RecvOut
