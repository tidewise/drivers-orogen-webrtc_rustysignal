# Signalling for Rock using rustysignal

rustysignal is a very simple signalling server. This project provides an
interface compatible with the protocol defined by `webrtc_base` going through
rustysignal.

The JSON document sent by the component is of the form

```
{
    to: "target_peer_id",
    protocol: "one-to-one" | "one-to-all",
    action: "request-offer" | "offer" | "answer" | "candidate" | "peer-disconnect" | "peer-disconnected",
    data: {
        from: "self_peer_id",
    }
}
```

In addition, offer and answer messages have a `data.description` field that
contains the SDP session description. `candidate` messages have a
`data.candidate` field with the ICE candidate description and a `data.mid` field
with the m-line number (as an integer). All other messages marshal their payload
as `data.message` (if they have one)

