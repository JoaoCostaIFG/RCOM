## Intro

**Circuit Switching** (same circuit for all packets): $T=Te+Tprop+Td$. $Te$
estabelecer ligação, $Td$ envio ($Length/Bitrate$).
**Packet Switching** (path is defined for each packet during travel):
$Tpac=Sum(Ti) \quad \vert \quad Ti=Tpi+Tdi$. $Ti$ envio packet in conn i
($Length/Bitrate$), $Tpi$ propagação in conn i.

## Physical Layer (Real comm channels used by the network. Appears as unreliable virt bit pipe.)

### Conversão de sinal digital s(t) num sinal analógico r(t) (convolução de s(t) com h(t))

Quantos mais harmónicos no sinal analógico, mais próximo do digital. Signal
de M níveis pode ser reconstruído, $C = 2B * log2(M)$. $C$, bitrate/channel's
capacity. $2B$, baudrate, samples/s, débito de símbolos.

### Baseband Transmission (Frequencies from zero up to a maximum $Bh$. Common for wires)

**NRZ-L** - 2 levels representing 0 and 1. **NRZ-I** - Change of level represents
a 1. **Manchester** - Transition in the middle of the bit. 1: positive - negative.
0: negative - positive. Used in Ethernet.  
**Clock Recovery** - Signals need sufficient transitions. Bad muitos 0's ou 1's seguidos.
Solved by **Manchester**.

### Passband (Band of frequencies around the frequency of the carrier, $fc$. Common for wireless and optical channels)

Uses modulation techniques: amplitude, frequency, and phase.

### Shannon's Law (Noise high $\Rightarrow$ low M. high SNR $\Rightarrow$ high M.)

Noise limits number of levels, M (bit/symbol). Maximum theorectical capacity
of a channel (bit/s): $C = B \times log2(1 + SNR)$. **B** - bandwidth of the
channel (Hz). **N** - White noise (W/Hz). **NB** - noise power seen by
receiver (W). **P** - signal power seen by receiver (W).  
**SNR** - Signal Noise Ratio = $P / (N * B)$.
**Free Space Loss**: $\frac{P_t}{P_r} = \frac{(4 \pi d)^2}{\lambda^2} = \frac{(4 \pi f d)^2}{c^2}$

## Data Link Layer (Service network layer. Errors. Regulate data flow)

**Services**: Unacknowledged connectionless service, Acknowledged connectionless
service, and Acknowledged connection-oriented service.
**Framing**: Character count, flag bytes with byte stuffing, or Start and
ending flags with bit stuffing.  
**Types of Errors**: Simple Error (random and independent from previous error),
Errors in burst (affects neighbour bits. Burst length defined by the first and
last bits in error).

### Counting Errors (Independent errors)

**BER** (Bit Error Ratio), bit error probability. **n**, frame length. **FER**
(Frame Error Ration), $1 - (1 - BER)^n$.  
**P[i bits received in error]**, $nCi * p^i * (1 - p)^{n - i} \quad \vert \quad
\frac{n!}{i! \times (n - i)!}$

### Effectiveness of Error Detection Techiniques

**Minimum distance of code**, d, is the minimum number of bit errors undetected
in a block of n bits. If fewer than d errors occur, errors are detected. **Burst
detecting ability**, B, is the max burst length of errors detected.

## ARQ

### Stop and Wait ARQ (Module 2 numbers. No NACK required.)

$Tf = \frac L R \quad \vert \quad
Tprop = \frac d V \quad \vert \quad
a = Tprop/Tf \quad \vert \quad
S = \frac{Tf}{2Tprop + Tf} = \frac{1}{1 + 2a} \quad \vert \quad
Serr = \frac{1 - p_e}{1 + 2a} \quad \vert \quad
R_{max} = R * S_{max}$

### Go-back-N ARQ (goes back to ask for missing frame)

Sequence numbers are represented module M, k bits are used for the sequence
numbers. $p_e = FER \quad \vert \quad W = M - 1 = 2^k - 1$

Eficiencia (S) without errors
$\begin{cases}$
$1, W \ge 1 + 2a\\$
$W/(1 + 2a), W < 1 + 2a$
$\end{cases}$
$\quad \vert \quad$
Eficiencia (S) with errors
$\begin{cases}$
$\frac{1 - p_e}{1 + 2a \times p_e}, W \ge 1 + 2a\\$
$\frac{W \times (1 - p_e)}{(1 + 2a)(1 - p_e + W \times p_e)}, W < 1 + 2a$
$\end{cases}$

### Selective-repeat ARQ (asks for the missing frame and saves others)

Adequate if W (a) is very large. $W = \frac{M}{2} = 2^{k - 1}$
$\quad \vert \quad S =$
$\begin{cases}$
$1 - p_e, W \ge 1 + 2a\\$
$\frac{W \times (1 - p_e)}{1 + 2a}, W < 1 + 2a$
$\end{cases}$

## Delay models

### Little's Theorem

The (mean) time a client has to wait before being served, $Tw$, depends on the
number of clients waiting, $Nw$, and on the arrival rate of the clients,
$\lambda$. $N$ - average number of clients in a system. $T_w$ - time a client
waits in queue. $N_w$ number of clients waiting in queue. $T_s$ - service time.
$N_s$ number of clients being served.
$N = \lambda T \quad \vert \quad
T = T_w + T_s \quad \vert \quad
N = N_w + N_s \quad \vert \quad
N_w = \lambda T_w$

### M/M/1 Queue

$N = \frac{\rho}{1 - \rho} = \frac{\lambda}{\mu - \lambda} \quad \vert \quad
T = N / \lambda = \frac{1}{\mu - \lambda} \quad \vert \quad
T_w = \frac{\rho}{\mu(1 - \rho)} \quad \vert \quad
N_w = T_w \lambda = N - \rho \quad \vert \quad
\mu = Cap/MedianLen \quad \vert \quad
\rho = \lambda / \mu \quad \vert \quad
T = N / \lambda$

#### M/M/1/B Queue

Packets can be lost ($P(B)$ probability).
$\rho(B) = \frac{1}{B + 1}, \rho = 1 \quad \vert \quad$
$\rho(B) \approx \frac{\rho - 1}{\rho} = \frac{\lambda - \mu}{\lambda}, \rho >> 1 \quad \vert \quad$
$\rho(B) = \frac{(1 - \rho) \times \rho^B}{1 - \rho^{B + 1}}$

#### M/G/1

Poison arrivals at rate $\lambda$. $T = T_w + 1/\mu \quad \vert \quad N = N_w + \rho$

#### M/D/1 (constant packet length)

Deterministic, constant service time, **Ts**, $1/\mu$.
$T_w = \frac{\rho}{2\mu \times (1 - \rho)}$.

#### D/D/1 vs M/M/1

Check Little's theorem. For the same $\rho$, D/D/1 has less clients, $N$,
waiting because they don't arrive in bursts.

## MAC (48 bit) - Medium Access Control

### ALOHA

- **Pure ALOHA (unslotted)** - No slot concept. Station transmits when it
  has a frame to transmit.
- **Slotted ALOHA** - Time divided into time slots. $T_{slot} = T_{frame}$.
  (Re)transmissions only at the beginning of a slot.

### CSMA (Carrier Sense Multiple Access)

Listen before transmission (if channel is busy, defer transmission). Collisions
can still occur (propagation delay). If collision, entire packet is lost.
Collision probability: $a = T_{prop} / T_{frame} << 1$.

- persistent - if busy, wait until free to transmit.
- non-persistent - if busy, wait a random time and repeat algorithm.
- p-persistent - if free, transmit with **p** probablity (otherwise defer). if busy,
  if transmission defered from previous time slot, collision, else wait until free
  and repeat algorithm.

### CSMA/CD (full-duplex ethernet => no collisions => method not used)

Collision Detection - stations listens to medium while transmitting. If collision
is detected: transmission is aborted, retransmission is delayed using a Binary
Exponential Back-off algorithm (after the ith consecutive collision wait a random
number of slots in $[0, 2^i - 1]$). Minimum frame size is required for collision
detection.

### CSMA/CA (Tframe >> Tprop)

Monitors channel activity until a certain idle period is detected. Transmit when
free. If busy, random backoff interval is selected. This interval is only decremented
if the sensed idle. Transmits when the backoff. Needs ACK by destination station.
**Station waits** random backoff time between two consecutive frames, even if detected
idle.

### Switch - frame forwarding/flooding

If entry in table: if destination is on segment from which frame arrived => drop
frame; else forward the frame on indicated interface. Else, forward on all but
the interface on which the frame arrived (**flood**).

## Network Layer (routing + switching. Addressing and congestion control.)

IP addresses mantêm-se. Mac address é os locais: source de onde veio na LAN e dest
para onde vai na LAN.

## Transport Layer (Transparent transfer data between hosts. E2E error recovery + flow control.)

Network congestion decreases $\Rightarrow$ CongestionWin increases (and vice-versa).  
$MaxWin = MIN(CongestionWin, AdvertisedWin) \quad \vert \quad
EffWin = MaxWin - (LastByteSent - LastByteAcked) \quad \vert \quad
Bitrate = CongestionWin / RTT$
