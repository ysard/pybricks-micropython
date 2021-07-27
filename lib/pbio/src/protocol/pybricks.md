<!--
This file requires https://mermaid-js.github.io for rendering diagrams.

Using <pre> tags will allow them to still render in a human-readable form when
mermaid is not available (and also prevents doxygen from corrupting the data).
DO NOT add empty lines inside of the <pre> blocks.
-->

# Pybricks Protocol

The Pybricks protocol is a collection of standard and custom Bluetooth Low
Energy services that runs on Powered Up hubs.

## Pybricks data streams

BLE does not have reliable transmission. Even if Write With Response and
Indications are used instead of Write Without Response and Notifications,
buffer overflows higher up in the stack can still cause data loss. So we
need an extra protocol layer that ensures no data is lost. This is particularly
important for streams of large chunks of data that don't fit in a single
packet.

### Stream metadata

In addition to sending raw binary data, addition information is needed to
detect and recover from data loss.

Each data packet includes a sequence number (`seq`) that is incremented by one for
each packet sent.

Additionally, status messages sent back to the sender from the receiver include
the last sequence number successfully received (`seq`) and the available space in the
receive buffer (`size`).

### Requesting a channel

To allow multiple simultaneous connections, each connection from a Central must
request a unique channel (`ch`). Channel 0 is connected to stdio. Additional channels
are user-defined.

Note that the peripheral sends the `max_pdu` size once with the first
`CHANNEL_STATUS` event. This is needed since some BLE APIs (e.g. WebBluetooth)
don't have access to this information.

<pre class="mermaid">
sequenceDiagram
participant c as Central
participant p as Peripheral
c->>p: request channel
note over c: cmd: REQUEST_CHANNEL, ch: 0
opt after timeout with no response
    c->>p: resend request
end
alt channel in use, invalid args, etc.
    p->>c: error response
    note over p: evt: CHANNEL_STATUS, ch: 0, last_seq: -1, err: ?
else
    p->>c: peripheral rx buffer status
    note over p: evt: CHANNEL_STATUS, ch: 0, last_seq: 0, size: 100, payload_size: 18
    opt timeout before receiving any more requests from central
        p->>c: resend rx buffer status
    end
    c->>p: central rx buffer status
    note over p: cmd: CHANNEL_STATUS, ch: 0, last_seq: 0, size: 100
    opt timeout before receiving any more requests from peripheral
        c->>p: resend rx buffer status
    end
end
</pre>

After this sequence is complete, both sides are ready to transfer data.


### Data transfer

Data transfer is bidirectional, so this works both ways (swapping commands for
events).

The sender may send as many payloads of `payload_size` that will fit in `size`
without waiting for a response. Then it must wait for a `CHANNEL_STATUS` event
before continuing.

To increase throughput, the receiver does not send a status event in response
to every payload received.

For example, if the receiver buffer can hold two full payloads, then it must
wait for the acknowledgement that the first payload was received before sending
the third payload.

<pre class="mermaid">
sequenceDiagram
participant c as Central/Peripheral
participant p as Peripheral/Central
c->>p: payload  1
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 1, payload: ...
c-->>p: additional payload(s)
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 2, payload: ...
p->>c: buffer status
note over p: evt: CHANNEL_STATUS: ch 0, seq: 1, size: ?
c->>p: next payload
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 3, payload: ...
p->>c: buffer status
note over p: evt: CHANNEL_STATUS: ch 0, seq: 2, size: ?
p->>c: buffer status
note over p: evt: CHANNEL_STATUS: ch 0, seq: 3, size: ?
</pre>

If data is lost (due to buffer overflow or BLE packet not received), the receiver
will respond with the last sequence number that was received correctly. Out of
order packets are also discarded. Senders must then resend data starting with
`seq + 1`.

<pre class="mermaid">
sequenceDiagram
participant c as Central/Peripheral
participant p as Peripheral/Central
c->>p: payload  1
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 1, payload: ...
c-xp: payload  2 (dropped)
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 2, payload: ...
c->>p: payload  3 (received, discarded)
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 3, payload: ...
p->>c: response to payload 1
note over p: evt: CHANNEL_STATUS: ch 0, seq: 1, size: ?
p->>c: response to payload 3
note over p: evt: CHANNEL_STATUS: ch 0, seq: 1, size: ?
c->>p: payload  2
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 2, payload: ...
c->>p: payload  3
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 3, payload: ...
p->>c: response to payload 2
note over p: evt: CHANNEL_STATUS: ch 0, seq: 2, size: ?
p->>c: response to payload 3
note over p: evt: CHANNEL_STATUS: ch 0, seq: 3, size: ?
</pre>

If status responses are dropped, there are several possibilities for recovery.
If a status response with a higher sequence number is received, then all is
well and no additional recovery is needed. The sender can continue to send
additional data. If no status messages are received after a timeout, the sender
must resend the data starting with `seq + 1` from the most recent status message
since in this case there is no way to tell if the data was dropped or the status
response was dropped. If the data was already received correctly, the receiver
will discard it and send another status event.

<pre class="mermaid">
sequenceDiagram
participant c as Central/Peripheral
participant p as Peripheral/Central
c->>p: payload  1
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 1, payload: ...
c-xp: payload  2
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 2, payload: ...
c->>p: payload  3
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 3, payload: ...
p->>c: response to payload 1
note over p: evt: CHANNEL_STATUS: ch 0, seq: 1, size: ?
p->>c: response to payload 2
note over p: evt: CHANNEL_STATUS: ch 0, seq: 2, size: ?
p-xc: response to payload 3 (dropped)
note over p: evt: CHANNEL_STATUS: ch 0, seq: 3, size: ?
c->>p: payload  4 (after timeout)
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 4, payload: ...
p->>c: response to payload 4
note over p: evt: CHANNEL_STATUS: ch 0, seq: 4, size: ?
</pre>

If the receiver is not draining the receive buffer, then a point can be reached
where the sender has received acknowledgements for all packets that it has sent
but there is no room to be able to send more data. To avoid a deadlock, the
receiver must send a status message again each time data is drained from the
buffer and there are no pending.

<pre class="mermaid">
sequenceDiagram
participant c as Central/Peripheral
participant p as Peripheral/Central
c->>p: request channel
note over c: cmd: REQUEST_CHANNEL, ch: 0
p->>c: peripheral rx buffer status
note over p: evt: CHANNEL_STATUS, ch: 0, last_seq: 0, size: 36, payload_size: 18
c->>p: central rx buffer status
note over p: cmd: CHANNEL_STATUS, ch: 0, last_seq: 0, size: ?
c->>p: payload  1
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 1, payload: ... (18 bytes)
c-xp: payload  2
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 2, payload: ... (18 bytes)
p->>c: response to payload 1
note over p: evt: CHANNEL_STATUS: ch 0, seq: 1, size: 18
p->>c: response to payload 2
note over p: evt: CHANNEL_STATUS: ch 0, seq: 2, size: 0
p->>p: drain 20 bytes from buffer
p->>c: response to payload 2
note over p: evt: CHANNEL_STATUS: ch 0, seq: 2, size: 20
c->>p: payload  3
note over c: cmd: CHANNEL_DATA: ch: 0, seq: 3, payload: ... (18 bytes)
p->>c: response to payload 3
note over p: evt: CHANNEL_STATUS: ch 0, seq: 3, size: 2
</pre>

### Algorithm

This demonstrates the state machine used to implement the protocol.

Note: devices can be disconnected at any time, so that needs to be handled as
well but is omitted from the diagrams for clarity.

#### Central

The main connection sequence is as follows:

<pre class="mermaid">
stateDiagram-v2
[*] --> send_ch_req
send_ch_req --> wait_seq_0 : request sent
wait_seq_0 --> send_ch_req : timeout (no status received)
wait_seq_0 --> err : too many timeout retries
state if_err &lt;&lt;choice&gt;&gt;
wait_seq_0 --> if_err : received status
if_err --> err : seq. -1 & error code
if_err --> send_seq_0 : seq. 0 & info
send_seq_0 --> tx_rx : request sent
tx_rx --> send_seq_0 : timeout (no data or status received)
tx_rx --> err : too many timeout retries
err --> [*] : return error code
</pre>

The `tx_rx` state consists of 3 parallel tasks.

<pre class="mermaid">
stateDiagram-v2
state tx_rx {
    [*] --> wait_tx_buf
    wait_tx_buf --> send_data : local > 0, remote > 0
    send_data --> wait_tx_buf : data sent
    \--
    [*] --> wait_status
    wait_status --> wait_status : received status
    \--
    [*] --> wait_rx_data
    wait_rx_data --> send_status : data received
    send_status --> wait_rx_data : status sent
}
tx_rx
</pre>

In more detail:

<pre class="mermaid">
%%{ init: { flowchart: { useMaxWidth: false } } }%%
flowchart TB
subgraph peripheral
p_start([start]) --> p_wait_ch_req[/wait for channel request command/]
p_wait_ch_req --> p_wait_ch_req_timeout{timed &lt;br&gt; out?}
p_wait_ch_req_timeout --> |yes| p_err
p_wait_ch_req_timeout --> |no| p_is_ch_req_valid{is &lt;br&gt; request &lt;br&gt; valid?}
p_is_ch_req_valid --> |no| p_send_err[/send error event/]
p_send_err --> p_did_send_err{did &lt;br&gt; send?}
p_did_send_err --> |no| p_err
p_did_send_err --> |yes| p_wait_ch_req
p_is_ch_req_valid --> |yes| p_send_seq_0[/send status event/]
p_send_seq_0 --> p_did_send_seq_0{did &lt;br&gt; send?}
p_did_send_seq_0 --> |no| p_err
p_did_send_seq_0 --> |yes| p_wait_seq_0[/wait for status command/]
p_wait_seq_0 --> p_wait_seq_0_timeout{timed &lt;br&gt; out?}
p_wait_seq_0_timeout --> |yes| p_retry_ch_req{retries &lt;br&gt; exceeded?}
p_retry_ch_req --> |no| p_send_seq_0
p_retry_ch_req --> |yes| p_err
p_wait_seq_0_timeout --> |no| p_is_seq_0{is seq. 0?}
p_is_seq_0 --> |no| p_wait_seq_0
p_is_seq_0 --> |yes| p_tx_rx[[transmit and receive]]
p_tx_rx --> p_err
p_err[set error code] --> p_end_([end])
end
subgraph central
c_start([start]) --> c_send_ch_req[/send channel request command/]
c_send_ch_req --> c_did_send_ch_req{did &lt;br&gt; send?}
c_send_ch_req -.-> p_wait_ch_req
c_did_send_ch_req --> |no| c_err
c_did_send_ch_req --> |yes| c_wait_seq_0[/wait for status event/]
p_send_err -.-> c_wait_seq_0
p_send_seq_0 -.-> c_wait_seq_0
c_wait_seq_0 --> c_wait_seq_0_timeout{timed &lt;br&gt; out?}
c_wait_seq_0_timeout --> |yes| c_retry_ch_req{retries &lt;br&gt; exceeded?}
c_retry_ch_req --> |no| c_send_ch_req
c_retry_ch_req --> |yes| c_err
c_wait_seq_0_timeout --> |no| c_is_seq_0_err{is &lt;br&gt; error?}
c_is_seq_0_err --> |yes| c_err
c_is_seq_0_err --> |no| c_send_seq_0[/send status command/]
c_send_seq_0 -.-> p_wait_seq_0
c_send_seq_0 --> c_did_send_seq_0{did &lt;br&gt; send?}
c_did_send_seq_0 --> |no| c_err
c_did_send_seq_0 --> |yes| c_tx_rx[[transmit and receive]]
c_tx_rx --> c_did_recv_any{did &lt;br&gt; receive data &lt;br&gt; or status?}
c_tx_rx --> c_err
c_tx_rx -.- p_tx_rx
c_did_recv_any --> |no| c_retry_seq_0{retries &lt;br&gt; exceeded?}
c_retry_seq_0 --> |no| c_send_seq_0
c_retry_seq_0 --> |yes| c_err
c_err[set error code] --> c_end_([end])
end
</pre>

<pre class="mermaid">
%%{ init: { flowchart: { useMaxWidth: false } } }%%
flowchart LR
subgraph transmit and receive
subgraph sender
direction TB
p_start([start]) --> p_end_([end])
end
subgraph receiver
direction TB
c_start([start]) --> c_end_([end])
end
end
</pre>
