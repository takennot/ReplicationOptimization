# Network Programming Assignment

Unreal Engine 5 C++ / Multiplayer networking / Legacy actor replication

This project is a small multiplayer networking benchmark that tests a simple idea: not every moving actor needs to be replicated equally often. The server simulates hundreds of moving cubes, and the project compares a uniform baseline against a prioritized policy that favors nearby and important actors. A client-side HUD records movement arrivals and network-driver byte/packet counters, then exports them to CSV.

The goal is to reduce server-to-client network traffic without treating all actors as equally important, while making the trade-offs observable. In a local testing, there was a reduction of approximately 60% in incoming bandwidth & packets; the exact figure, metric, and conditions can be found in the attached spreadsheet.

# Scope

This project uses Unreal's legacy `UNetDriver` actor-replication path, not Iris, Replication Graph, or a custom packet transport.
Comparison with Iris will be added in near future.

## 1. Run the project

### Requirements

- Windows with Unreal Engine 5.8.

### Setup

1. Open `ReplicationBudgeter.uproject` and compile the missing C++ modules if prompted.
2. The project initially targets `/Engine/Maps/Entry`. If that map is unavailable in your engine installation, create an Empty Level and press Play; `ARBGameMode` generates the floor, light, and replicated test actors.
3. In the Play dropdown, choose Number of Players: 2, Net Mode: Play As Listen Server, and New Editor Window (PIE). Run Under One Process is convenient for initial testing, although separate processes are probably more realistic.
4. Press Play. One window is the listen server (which also has a local player); the other is a remote client. Use WASD + mouse to move the default flying pawn.
5. Record incoming replication metrics on the remote client window. The listen server does not receive its own actors' network movement updates like a remote client does.

### Controls

| Key / console command | Effect |
|---|---|
| `P` | Toggle baseline / prioritized replication, requested through a reliable server RPC |
| `1` / `2` / `3` | Respawn 50 / 200 / 500 test actors via the server |
| `V` | Toggle local debug wireframes |
| `RBSetMode 0` / `RBSetMode 1` | Explicitly select baseline / prioritized mode |
| `RBSetCount 300` | Set a custom actor count (clamped to 10-1000) |
| `RBClearSamples` | Clear local HUD/CSV samples and restart the local measurement interval |
| `RBExportCSV` | Export this window's captured samples to `Saved/Benchmarks/RB_YYYYMMDD_HHMMSS.csv` |

Open the in-game console with `~` (tilde). Commands that change the scenario affect the server through RPCs, even when entered on a client. `RBClearSamples` and `RBExportCSV` operate on the window where you execute them.

Wireframes are neutral in baseline mode. In prioritized mode they are cyan (near), gold (middle), gray (far), and magenta (critical). These colors show categories, not packet-level scheduling decisions.

## 2. What was implemented?

The demo adds a repeatable simulation and two separate policy layers on top of Unreal's normal replication system:

1. Server-global update-frequency selection: periodically changes when actors can be considered for replication.
2. Per-connection priority weighting: changes which eligible actors are relatively more important for a given client's connection.

It also adds server-authoritative controls, client-side replication measurements, a debug HUD, CSV export, and unit-testable policy functions.

| Source file / class | Main responsibility |
|---|---|
| `RBGameMode.cpp` / `ARBGameMode` | On the server, spawns actors and the stage; recalculates update rates every 0.5 s; applies requested policy and actor-count changes |
| `RBProbeActor.cpp` / `ARBProbeActor` | Continuously moves cubes on the server; enables actor/movement replication; overrides `GetNetPriority()`; counts received movement notifications on clients |
| `RBPolicy.cpp` | Pure C++ functions deciding target replication frequency and priority multipliers |
| `RBPlayerController.cpp` | Player input, console commands, and reliable client-to-server RPCs for changing the test |
| `RBGameState.cpp` | Replicates current mode and configured actor count from the server to clients |
| `RBHUD.cpp` | Local HUD, approximately one-second sampling, network-counter calculations, CSV export |
| `RBNetRateMath.h` | Computes per-second byte/packet rates from consecutive cumulative counter readings |
| `RBStage.cpp` | Runtime-generated floor and directional light |

### Client/server and authority decisions

- The server is authoritative. Each `ARBProbeActor` advances along its sine/cosine movement path every server tick. Clients receive transforms using `SetReplicates(true)` and `SetReplicateMovement(true)`; they do not authoritatively simulate those cubes.
- Clients request, the server decides. Pressing `P`, `1`, `2`, or `3` calls a reliable server RPC on the owning `ARBPlayerController`. The server then changes the policy or respawns the scenario through `ARBGameMode`.
- Clients learn which experiment is active. `ARBGameState` replicates `bOptimized` and `ScenarioActorCount`, allowing the HUD to label samples correctly.
- Measurement is local to each window. `OnRep_ReplicatedMovement()` runs when a client receives replicated movement, and the HUD reads its own world's net driver. This is why client incoming bandwidth is the main figure for a single remote-client test.
- Normal Unreal relevancy and sending still apply. The project does not replace the network driver or guarantee every actor will be delivered at the target rate.

Data flow: server moves cubes --> server chooses eligible update rate --> legacy replication considers/ranks actors per connection --> Unreal sends changed actor state --> remote client applies movement --> client HUD records arrivals and network traffic.

## 3. How does the optimization work?

### A. Update frequency: when might this actor be sent?

Every 0.5 seconds, `ARBGameMode::ApplyRates()` measures each cube's distance to the closest player pawn anywhere on the server and calls `RBPolicy::ChooseUpdateHz()`.

In baseline mode, every cube targets 20 Hz, regardless of importance or distance. In prioritized mode, the target is:

| Nearest player's distance | Ordinary cube | Critical cube |
|---|---:|---:|
| Under 18 m | 20 Hz | 20 Hz |
| 18 m to under 45 m | 8 Hz | 12 Hz |
| 45 m or more | 2 Hz | 12 Hz |

Unreal stores distances in centimeters in this project: the thresholds are 1,800 cm and 4,500 cm. Cubes whose `ProbeId` is divisible by 10 are marked critical, so around 10% of them receive the higher importance treatment.

A target of 20 Hz corresponds to a 50 ms interval, 8 Hz to 125 ms, and 2 Hz to 500 ms. These are replication consideration targets, not guarantees of transmitted updates. The net driver can skip updates because of scheduling, relevancy, saturation, or unchanged properties.

Why it helps: When a distant cube is still moving on the server but is considered for replication less often, fewer changed transforms generally need to travel over the network. For 100 ordinary faraway cubes, dropping the *target* from 20 Hz to 2 Hz changes the theoretical combined consideration rate from 2,000 to 200 per second. That does not imply 90% less total bandwidth, because other replicated state, packets, and overhead still exist.

Important limitation: This rate is server-global, not independently chosen for each client. If *any* player is near a cube, the cube can keep a high update rate even when other players are far from it.

### B. Priority: *which eligible actor matters more to this client?*

`ARBProbeActor::GetNetPriority()` retains Unreal's normal computed priority in baseline mode. In prioritized mode it multiplies that priority by three weights:

`Final priority = Unreal base priority * distance weight * critical-object weight * waiting-time weight`

| Factor | Multiplier in the source |
|---|---:|
| Viewer nearer than 18 m | ×3.0 |
| Viewer 18-45 m away | ×1.5 |
| Viewer 45 m or farther away | ×0.45 |
| Critical cube | Additional ×2.5 |
| Waiting-time weight | ×1.0 to ×4.0 |

The waiting-time weight is `clamp(time_since_last_replication / 0.2, 1.0, 4.0)`. A cube that has waited longer gains some priority, reducing the risk of constantly ignoring distant cubes. Unreal's base priority already includes its own time-related behavior; the project's additional multiplier is capped and does not guarantee starvation freedom.

Unlike update frequency, this calculation uses the current connection's viewer position. An actor can therefore rank highly for a nearby player and lower for a distant player. The result is a relative weight, not a fixed bandwidth allocation or a promise that the actor will be sent immediately.

The central distinction: `NetUpdateFrequency` controls *when an actor is considered*, while `GetNetPriority()` influences *which considered actors get resources on a particular connection*. Most of the measurable byte savings may come from reducing update frequency; a priority change alone might mainly redistribute limited bandwidth.

## 4. Benchmarking and network conditions

### Previous benchmark results

In total there have been 6 benchmarks:
1. Baseline mode with 200 actors, 30 seconds of capture, limited bandwidth (12000), 10 ms ping
2. Prioritized mode with 200 actors, 30 seconds of capture, limited bandwidth (12000), 10 ms ping
3. Baseline mode with 500 actors, 60 seconds of capture, relaxed bandwidth (1000000), 10 ms ping
4. Prioritized mode with 500 actors, 60 seconds of capture, relaxed bandwidth (1000000), 10 ms ping
5. Baseline mode with 500 actors, 60 seconds of capture, relaxed bandwidth (1000000), 300 ms ping
6. Prioritized mode with 500 actors, 60 seconds of capture, relaxed bandwidth (1000000), 300 ms ping

First 2 tests showed a reduction of 2-4% in incoming traffic, which I considered to be a statistical error.
Later tests with more actors, longer capture time & higher bandwidth showed an improvement of around 60% in incoming traffic.
A more detailed spreadsheet you can find here: [LINK THE SPREADSHEET HERE]

### Benchmarking process

The way I have done the benchmarking is this:

Use the same settings and player positions for both modes. A reliable starting test is two-player PIE with one listen server and one remote client, at 500 cubes.

1. Start the project as described above. Put the players in fixed positions; avoid moving the camera/pawns between the two captures.
2. In either window, set the actor count (`RBSetCount 500`, for example). Wait for the new actors to arrive; verify `observed_actors` is close to `configured_actors` on the remote client.
3. Set baseline with `RBSetMode 0`. After allowing at least 5-10 seconds for the mode and actor spawning to settle, run `RBClearSamples` on the remote client.
4. Record 30-60 seconds without changing the scene. Run `RBExportCSV` on that client; note the generated filename.
5. Set prioritized with `RBSetMode 1`. Wait again, clear client samples, run for the same duration, and export a second CSV.
6. Compare averages from valid steady-state rows in each CSV. Ignore the first row with blank network-rate fields. 

Repeating the benchmark with different conditions is probably a good idea to get more credible results.

A mode switch resets neither the actors' lifetime reception counters nor every network-internal state; `RBClearSamples` resets the HUD's samples and differential-counter baseline. That is another reason to allow a settle period and exclude transition rows.

### Network settings: what is known, and what still needs recording

The default project settings can be found in `Config/DefaultEngine.ini`:

```ini
[/Script/OnlineSubsystemUtils.IpNetDriver]
MaxClientRate=1000000
MaxInternetClientRate=1000000

[/Script/Engine.Player]
ConfiguredInternetSpeed=1000000
ConfiguredLanSpeed=1000000
```

These are configured connection-rate/speed settings (bytes per second), not proof that every test actually transferred exactly 1,000,000 bytes per second.

### Reported results and how to interpret them

For each test, compute the average of the client's `in_bytes_per_s` over valid rows. The bandwidth reduction is:

Reduction (%) = (mean_baseline_in_bytes_s - mean_prioritized_in_bytes_s) / mean_baseline_in_bytes_s * 100

### Issues

Issue: The first baseline/prioritized comparison showed only about 2-4% lower incoming traffic and movement-event rate, despite a policy intended to throttle many distant actors.

Fixing the issue: The project used relatively restrictive connection-rate settings (`12000` in `DefaultEngine.ini`). Under a constrained connection, Unreal may already be selecting among competing actors rather than sending all possible movement updates. This makes a difference between baseline and prioritized traffic harder to isolate. The next diagnostic steps were to increase the test load to 500 actors, capture data for a longer time and compare both modes under a higher configured connection rate (`1000000`).

Post-fix observation: A later test was reported to show roughly 50-60% lower traffic.

Lesson: Before concluding that a replication optimization is ineffective, check whether the existing connection limits or test conditions are masking its impact.

## 5. CSV fields explained

`RBHUD` samples about once per second. For network byte/packet rates, it reads cumulative `UNetDriver` counters and divides their change by the elapsed time. The output file lives under `Saved/Benchmarks/`.

| CSV column | What it means and why it matters |
|---|---|
| `client_time_s` | The sampling world's local Unreal time in seconds. Useful for ordering samples and separating warm-up from steady-state. It is not synchronized wall-clock time, even for a server-side export. |
| `capture_side` | `client` or `server`: which window captured the row. Essential for interpreting *in* versus *out*. |
| `policy` | `baseline` or `prioritized`, read from the replicated GameState. Prevents mixing the two modes in one analysis. |
| `configured_actors` | The server-reported intended scenario size. Keep it equal between baseline and prioritized trials. |
| `observed_actors` | Number of probe actors currently present in this window's world. Checks whether a client has actually received a comparable actor set. |
| `actors_with_movement_samples` | Number of observed probes that have received at least one movement notification since they were spawned, not necessarily in the current measurement interval. Helps assess coverage; actors with none are excluded from the mean-age calculation. |
| `movement_events_per_s` | Number of `OnRep_ReplicatedMovement()` callbacks across probes per second. Measures changed-movement arrival cadence, not packets or bytes. Often lower in prioritized mode; that is a trade-off as well as a potential saving. |
| `mean_age_since_event_s` | Mean time since the latest movement notification among actors that have received one. A freshness/staleness indicator. May rise when distant updates are reduced. |
| `client_ping_ms` | Owning player's reported ping in milliseconds. Check that conditions are similar in both tests. |
| `net_sample_duration_s` | Time between consecutive driver-counter readings; often approximately one second. Used as the denominator when deriving traffic rates. |
| `in_bytes_per_s` | Aggregate incoming net-driver bytes per second. Primary bandwidth metric on the remote client, including traffic beyond probe movements. Divide by 1024 for KiB/s. |
| `out_bytes_per_s` | Aggregate outgoing net-driver bytes per second. Useful on the server; on the client it measures client-to-server traffic rather than received cube movement. |
| `in_packets_per_s` | Received packets per second. A lower rate can indicate fewer packets, but not all packets contain the same number of bytes. |
| `out_packets_per_s` | Sent packets per second. Helps examine the sending side, especially server-to-client traffic on a server export. |
| `net_actor_channels` | Currently open actor channels on the connection (client) or aggregated across server client-connections. Includes more than test cubes; not the number of actors classified as high priority. |

Expected trade-offs: In prioritized mode, expect fewer movement notifications and ideally lower client incoming bytes; incoming packet counts may also decrease. `mean_age_since_event_s` may increase for actors updated less often. `configured_actors` and `observed_actors` should remain comparable, and ping should be similar. This CSV does not measure separate near/far update-age distributions, visual smoothness, positional error, or server CPU time.

## 6. Known limitations and next steps

- Legacy only: This does not implement Iris `UNetObjectPrioritizer`, Replication Graph, or its own packet scheduler.
- Not a strict bandwidth budget: The project adjusts eligibility and relative priority; Unreal still performs actual relevancy checks, batching, and packet scheduling.
- Server-global frequency: An actor close to one player may still update frequently for distant clients. A stronger engine-oriented version would do per-connection frequency/budget decisions.
- Limited smoothness handling: Clients do not implement custom interpolation or prediction for the cubes. Lower update frequencies can make distant movement visibly jumpy.
- Limited quality metrics: The CSV reports global arrival events and average notification age, not positional error or whether near/critical actors individually stay responsive.
- Limited performance metrics: Network bytes/packets are totals for the whole driver. The CSV cannot attribute each saved byte to an actor type or show server CPU cost; use Networking Insights and Unreal Insights for that.
- Heuristic weights: The x3.0 / x1.5 / x0.45 and critical/age multipliers are simple prototype choices, not production values.
- One-machine PIE is a starting point: A repeatable local experiment is useful, but further testing should include separate processes, a dedicated server, packet loss, latency, multiple client positions, and repeated trials.

Potential extensions: a per-connection rate scheduler, near/middle/far age histograms, a server-versus-client position-error metric, an explicit byte budget, and automated repeated benchmarks with saved run settings & comparison with Iris.
