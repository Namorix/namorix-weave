---
name: "BR Provisioning — Add Border Router (pending/accept/reject + identity)"
overview: "Luồng Add Border Router: BR advertise mDNS + mở TCP listener; backend browse mDNS + connect-out, quản lý vòng đời thiết bị qua SQLite/EF Core schema protocol-agnostic (Thread/Zigbee — chỉ Thread làm trước), identity ECDSA TOFU + client-assertion JWT, UI tái dùng pattern Joiner. Chia theo batch."
todos: []
isProject: true
status: in-progress
---

# BR Provisioning — Add Border Router

> **Bối cảnh:** 3 quyết định thiết kế đã chốt:
> 1. **Luồng BR tự "gõ cửa"**: connect → backend tạo record `Pending` → UI liệt kê → Accept (đặt tên + gán dataset → `Connected`) / Reject (giữ row `Rejected` + đóng socket + mã lỗi để firmware backoff dài). Bỏ hẳn hướng Scan/Nhập tay.
> 2. **Identity**: EUI-64 tự khai, dễ spoof → **ECDSA P-256** (sinh lúc factory, private key trong NVS encrypted/eFuse, không rời thiết bị). Accept chốt `(Eui64, PublicKey)` bất biến (TOFU). Sau accept, reconnect dùng client-assertion JWT (pattern OAuth2, đổi sang ES256).
> 3. **Persistence**: **SQLite + EF Core** — **DB riêng biệt** với core (file `weave.db` riêng, migration history riêng), schema **protocol-agnostic** (`Network` base + bảng detail 1:1 theo protocol). Không dùng prefix `Wv` vì namespace + DB đã tách hẳn. Zigbee **chưa làm** nhưng model đã dựng sẵn (ESPHome đã loại, không scaffold).

## ⚠️ Mâu thuẫn kiến trúc phải chốt trước (Phase 0) — ✅ ĐÃ CHỐT hướng B (HA-style)

Thiết kế ban đầu giả định **"backend listener + BR client connect-out"** — nhưng sau khi user chốt theo mô hình ESPHome/HA (**hướng B**), hướng ngược lại được chọn:

| | Quyết định cuối (hướng B) |
|---|---|
| Kẻ advertise | **Firmware** — mDNS `_thread-border-router-frame._tcp` + TCP server (listen `CONFIG_BR_FRAME_TCP_PORT` :5150, accept-1) |
| Kẻ browse/connect | **Backend** — mDNS browse (`BrMdnsBrowser`, Makaretu) → connect-out `BrTcpClient` (reconnect backoff) |
| Backend phát hiện BR | Tự browse mDNS, không cần config tĩnh (không `BorderRouter:Host`) |

**Transport cuối:** BR quảng bá + mở listener; backend browse rồi connect ra. Frame protocol giữ nguyên (`FrameCodec`/SOF/CRC-8/ACK/NACK). Batch 1 ban đầu (backend listener + firmware client) đã bị đảo ngược vì sai hướng so với mô hình HA.

---

## Kiến trúc mục tiêu

```
[ESP32-S3 BR]  --mDNS `_thread-border-router-frame._tcp` (quảng bá) + TCP server :5150-->
        ^
        | TCP :5150 (backend connect-out, retry backoff)
[Backend] — browse mDNS (Makaretu) → BrTcpClient per BR
   └─ Connected → handshake:
        · Thread: backend gửi `CMD_MAC_ADDRESS` → BR trả 8-byte MAC → Eui64
        · Backend lookup Network theo Eui64 (unique index):
            - `Connected` → pin identity → mở data channel, poll + SignalR push
            - `Pending`   → giữ record, chờ admin (chưa join data thật)
            - `Rejected`  → đóng socket + mã lỗi → firmware backoff dài
            - chưa biết   → tạo row `Pending` (TOFU)
        · Zigbee → (sau này) đường connect khác, `Host` = serial path
   └─ persistence: SQLite `weave.db` (EF Core) — `Network` + detail tables
        |
        v
[UI "Add border router"]  -- SignalR `/hubs/weave` --
   ├─ list devices (tái dùng NmxDataTable pattern ThreadJoinerPanel)
   ├─ Accept → đặt tên + tạo ThreadDataset → `Connected`
   └─ Reject → giữ row `Rejected` (Eui64 bị chặn tự động)
```

**Boundary lúc Pending:** chỉ trao đổi identity (Eui64 + pubkey), **chưa** push data thật, chưa gán dataset. Admin duyệt xong mới được poll/join.

---

## Batch 1 — Đảo chiều transport về HA-style (BR advertise mDNS + backend browse/connect-out) — ✅ XONG

> **Đã xong (2026-08-14).** Sau khi user chốt hướng **B (ESPHome/HA)**, Batch 1 ban đầu (backend listener + firmware client) đã bị **đảo ngược**: khôi phục hướng committed (firmware server + mDNS advertise), rồi thay discovery config tĩnh bằng **mDNS browse** + quản lý **multi-BR**.
>
> **Backend:** `BrMdnsBrowser` (mới, Makaretu.Dns.Multicast) browse `_thread-border-router-frame._tcp` → event `BrFound/BrLost(BrEndpoint)`; `BrConnectionService` (hosted) mỗi BR browse được → 1 `BrTcpClient` (connect-out + reconnect backoff), trên `Connected` → `RegisterConnectionAsync` (handshake Eui64 qua `CMD_MAC_ADDRESS` → row `Network` `Pending`) → `ActiveBr`. Xóa `BrTcpServer`/`BrConnection` (Batch 1 sai hướng).
>
> **Firmware:** restore committed — `frame_tcp.c` server (listen `CONFIG_BR_FRAME_TCP_PORT` :5150, accept-1, state watchdog), `main.c` mDNS (`mdns_service_add("_thread-border-router-frame", "_tcp")`), Kconfig `BR_FRAME_TCP_PORT` default **5150**; bỏ `CONFIG_BR_BACKEND_HOST/PORT`.
>
> **Đã kéo DB slice tối thiểu lên Batch 1** để verify "row Pending trong DB" khả thi: `Models/Enums.cs`, `Models/Network.cs` (chưa có detail tables), `Persistence/WeaveDbContext.cs`, `Persistence/WeaveDbContextFactory.cs`, `Migrations/InitialCreate`. Direction-agnostic, giữ nguyên.
>
> **Độ lệch so với plan:** `CMD_STATE` keepalive poll cho **mọi** kết nối (không chỉ `Connected`) — firmware watchdog restart BR nếu 5×15s không nhận STATE. Data nặng (health/dataset/tables/state) vẫn chỉ khi `Connected`.
>
> **Config breaking:** `BorderRouter:Host` + `ListenPort` bỏ → `MdnsServiceName` (`_thread-border-router-frame._tcp`) + `FramePort` (**5150**, fallback khi SRV thiếu port — khớp `CONFIG_BR_FRAME_TCP_PORT`). **Docker `network_mode: host`** — multicast mDNS không xuyên bridge.
>
> **Open:** `WeaveDbContextFactory` (design-time) fallback `Data Source=weave.db` (CWD) chưa trỏ về data dir — chờ user chốt fix sang `NMX_DATA_DIR` hay không. **Core addon-manager path** đặt `NetworkMode = spec.NetworkName` (bridge) — chưa hỗ trợ host network khi weave được core launch; follow-up riêng, out of scope weave.

### Backend
- **`BrOptions.cs`** — bỏ `ListenPort`; thêm `MdnsServiceName` (`_thread-border-router-frame._tcp`) + `FramePort` (:5150); giữ `StatePollIntervalSec`/`RequestTimeoutMs`.
- **`BrMdnsBrowser.cs`** (mới) — browse mDNS (Makaretu `ServiceDiscovery` + `MulticastService`), dedupe theo instance, sweep 15s + expire 60s → `BrFound/BrLost`.
- **`BrTcpClient.cs`** — restore committed (connect-out + reconnect backoff 1s→30s).
- **`Program.cs`** — `BrMdnsBrowser` singleton thay `BrTcpServer`; giữ `BrProvisioningService` singleton + `AddHostedService<BrConnectionService>` + DB wiring.
- **`BrConnectionService.cs`** — multi-BR (1 client/BR), poll/push gate theo Status: chỉ `Connected` được đẩy SignalR data.
- **`appsettings.json`** — `BorderRouter`: `MdnsServiceName` + `FramePort`.
- **`Namorix.Weave.csproj`** — + `Makaretu.Dns.Multicast` 0.27.0.

### Firmware (`border-router-host`)
- **`transport/frame_tcp.c` / `.h`** — restore committed server: listen `CONFIG_BR_FRAME_TCP_PORT`, accept-1, RX dispatch, state watchdog.
- **`main.c`** — restore mDNS advertise `_thread-border-router-frame._tcp` + `frame_tcp_init()` sau backhaul IPv4; bỏ block client-connect.
- **`include/br_config.h`** — restore `TASK_NAME_TCP_ACCEPT/TCP_RX/STATE_WD`; bỏ `CONFIG_BR_BACKEND_HOST/PORT`.
- **`main/Kconfig.projbuild`** — restore `BR_FRAME_TCP_PORT`; bỏ `BR_BACKEND_HOST/PORT`.
- **`main/CMakeLists.txt` + `idf_component.yml`** — thêm lại `espressif/mdns`.

### Docker
- **`frontend/docker-compose.yml`** — `network_mode: host`; bỏ `ports` mapping (host network bind trực tiếp).

**Verify Batch 1:** ✅ **XONG (2026-08-14, user xác nhận).** Backend `dotnet build` sạch; firmware đã build + flash. `avahi-browse -rt _thread-border-router-frame._tcp` thấy BR → backend browse → connect → handshake Eui64 → row `Network` status `Pending` đã ghi trong `data/weave.db`. UI chưa cần.

---

## Batch 2 — DB layer (SQLite/EF Core) + pending/accept/reject + blacklist

### Models — `backend/src/Models/` (đã có `Enums.cs`, `Network.cs` từ Batch 1)
> Schema protocol-agnostic: 1 row `Network` cho mỗi network/gateway bất kể protocol; dữ liệu riêng của từng protocol nằm ở bảng detail 1:1. **Đã chốt: chỉ làm `BrThreadDataset` — `ZigbeeCoordinator` deferred (user, 2026-08-14).**

- **`Enums.cs`** ✅ (Batch 1)
```csharp
public enum Protocol { Thread = 0, Zigbee = 1 }
public enum NetworkStatus { Pending = 0, Connected = 1, Offline = 2, Rejected = 3 }
```
- **`Network.cs`** ✅ (Batch 1 + B2) — base entity: `Id`, `Protocol`, `Name`, `Host`, `Status = Pending`, `Eui64`/`PublicKey` (nullable, Thread-only), `FirstSeenAt`/`AcceptedAt`/`RejectedAt`/`CreatedAt`. **Thêm nav 1:1 `BrThreadDataset? ThreadDataset`** (B2). **Class giữ `Network`, bảng = `Networks`** (tên DbSet `Networks` — convention namorix, không `ToTable`, **bỏ hẳn prefix `Br`**).
- **`BrThreadDataset.cs`** ✅ (Batch 2, mới) — **class giữ `BrThreadDataset`, bảng = `BrThreadDataset`**. 1:1 với `Network`, **PK/FK = `NetworkId`**: `PanId`, `ExtendedPanId`, `Channel`, `ChannelMask`, `NetworkName`, `MeshLocalPrefix`, `NetworkKeyEncrypted` (**encrypt at rest** qua `WeaveSecretProtector`/DataProtection — không lưu plaintext), `Pskc`, `SecurityPolicy`.
- **`ZigbeeCoordinator.cs`** ⏳ deferred — chưa làm.

### Data — `backend/src/Persistence/` (đã có `WeaveDbContext.cs`, `WeaveDbContextFactory.cs` từ Batch 1)
- **`WeaveDbContext.cs`** ✅ (B1 + B2) — DbContext **riêng biệt** với core. `OnModelCreating`: enum `HasConversion<string>()`; **unique index `Eui64` + partial filter `IS NOT NULL`**; 1:1 `Network` ↔ `BrThreadDataset` (FK=PK `NetworkId`, **cascade delete**). DbSet: `DbSet<Network> Networks` (→ bảng `Networks`), `DbSet<BrThreadDataset> BrThreadDataset` (→ bảng `BrThreadDataset`).
- **`WeaveDbContextFactory.cs`** ✅ (Batch 1) — design-time factory cho `dotnet ef` CLI (env `WEAVE_DB_CONNECTION`, fallback `Data Source=weave.db` — open: có nên trỏ về `NMX_DATA_DIR`).
- **`Services/WeaveSecretProtector.cs`** ✅ (Batch 2, mới) — bọc `IDataProtectionProvider` (pattern `BcnSecretProtector` của Namorix): `CreateProtector("Weave.ThreadDataset")`, Protect/Unprotect idempotent (Magic prefix `CfDJ8`). Keyring DataProtection trỏ `addon.DataDir/keys` (Program.cs) — độc lập desktop. Phục vụ `NetworkKeyEncrypted` lúc Accept.

### Wiring — `Program.cs` ✅ (đã làm ở Batch 1)
> **Đường dẫn DB theo `DataDirectory` của core** (`Namorix.Core.IO.DataDirectory`) — weave đã `ProjectReference` `Namorix.Core.csproj`. Base path lấy từ `NmxAddonConfig.DataDir` (env `NMX_DATA_DIR`, default `./data`) — **cùng thư mục chứa `oauth.json`**, nên DB nằm trong persisted volume của addon, tách hẳn khỏi core. Không phát minh env mới.

```csharp
using Namorix.Core.IO;
using Namorix.Core.OAuth;

var addon = NmxAddonConfig.FromEnvironment();          // NMX_DATA_DIR → DataDir
builder.Services.AddSingleton(new DataDirectory(addon.DataDir));

var dbPath = Path.Combine(addon.DataDir, "weave.db");
builder.Services.AddDbContextFactory<WeaveDbContext>(options =>
    options.UseSqlite($"Data Source={dbPath}"));
// ...
using (var scope = app.Services.CreateScope())
{
    await using var db = await scope.ServiceProvider
        .GetRequiredService<IDbContextFactory<WeaveDbContext>>()
        .CreateDbContextAsync();
    db.Database.Migrate();   // single-instance addon → apply on startup, không lo rolling deploy
}
```
> Dùng `AddDbContextFactory` (không phải `AddDbContext`) vì `BrProvisioningService` là **singleton** (resolve từ hosted service) — mỗi lệnh mở context mới, tránh scoped-from-singleton. `NmxAddonConfig.FromEnvironment()` idempotent (đã được `AddNmxOAuth2Client()` gọi) — gọi lại để có `DataDir` cho connection string lúc registration là an toàn. DB + `oauth.json` cùng thư mục → redeploy weave không đụng core, core restart không đụng file này.

### Migration
- ✅ **`20260814022928_InitialCreate.cs`** — một migration tạo **cả 2 bảng** `Networks` + `BrThreadDataset` (PK/FK `NetworkId`, FK cascade, unique index `Eui64` partial `IS NOT NULL`). **Đã apply** (`make db-update`, verified 2026-08-14 — `__EFMigrationsHistory` có `20260814022928_InitialCreate`, ProductVersion 10.0.9; row `Network` `Pending` trong `src/data/weave.db`). Migration `AddThreadDataset` cũ (đang rename `Networks→BrNetwork`) đã bị xoá, sinh lại gọn thành InitialCreate này.

### Accept/Reject + blacklist
- **Accept** (Pending → Connected): admin đặt tên + chọn dataset → **tạo `ThreadDataset`** (admin nhập hoặc copy dataset active) → backend đẩy qua `SetPanId`/`SetChannel`/`SetNetworkName`/`SetExtendedPanId`/`SetNetworkKey` → `StartThread` → chốt `AcceptedAt`.
- **Reject** (Pending → Rejected): **giữ row** (không xoá), set `Status = Rejected` + `RejectedAt` → đóng socket + mã lỗi. Reconnect sau đó lookup trúng Eui64 unique index → **short-circuit**, không tạo row Pending mới mỗi retry.
- **Disconnect** (Connected → Offline): khi socket đóng, cập nhật `Status = Offline` (không phải Rejected).

**Verify Batch 2:** kịch bản — BR lạ connect → `Pending`; Accept → `Connected` + `ThreadDataset` gán; disconnect → `Offline`; Reject → row `Rejected`, BR reconnect không tự tạo lại Pending.

---

## Batch 3 — UI "Add border router" (frontend)

> **✅ XONG toàn bộ (2026-08-15, v0.5.0).** Mục 1–4: types + SignalR constants + store devices + hook. Mục 5–8: **Accept/Reject chuyển sang REST** (thay hub invoke, theo convention namorix) — backend `Controllers/NetworkController.cs` (`POST /api/networks/{id}/accept|reject`, `[RequireAuth]`, `ApiResponse<NetworkDto>`) + frontend `controllers/network.controller.ts` (`coreConfig.http`; `signalr/provisioning.ts` đã xoá); `BrProvisionPanel` (`NmxGrid` cards + Accept/Reject dialog); wire `NetworkView` list + `ThreadNetworkView` detail; i18n `en.json`. Mục 9: version bump **0.4.2 → 0.5.0**.

- **`frontend/src/types/network.ts`** ✅ — mirror DTO:
```ts
type NetworkStatus = "Pending" | "Connected" | "Offline" | "Rejected"
interface Network { id: number; protocol: string; name?: string; host?: string;
  status: NetworkStatus; eui64?: string; publicKey?: string;
  firstSeenAt?: string; acceptedAt?: string; rejectedAt?: string }
```
  (`protocol: string` mirror `NetworkDto.Protocol` string, không dùng literal union — Zigbee chưa có; có thể siết lại khi thêm protocol mới. `ThreadDataset` frontend type đã tồn tại trong `network.ts` — tái dùng/extension.)
- **`frontend/src/signalr/constants.ts`** ✅ — `WeaveSignalREvents` + `NetworkList` (`:network-list`) + `NetworkChanged` (`:network-changed`).
- **`frontend/src/hooks/useNetworks.ts`** ✅ (mới, pattern `useOtbrData`) — `useSignalREvent(WeaveSignalREvents.NetworkList/NetworkChanged)` → `networkActions.setNetworkList`/`upsertNetwork`.
- **`frontend/src/store/slices/networkSlice.ts`** ✅ — + `devices` normalized table (`Record<number, Network>` + `order: number[]`) + reducer `setNetworkList` (replace) / `upsertNetwork` (upsert 1 row).
- **`frontend/src/store/selectors/networkSelectors.ts`** ✅ — `selectNetworks` (ordered) + `selectNetworkCountByStatus` (badge count theo status).
- **Panel mới** trong **`NetworkView.tsx`** — tái dùng pattern `ThreadJoinerPanel.tsx` (`NmxCardContainer` + `NmxAlign` + `NmxButton` "Add border router" + `NmxCardSection` + `NmxDataTable`):
  - Cột: protocol badge (hiện chỉ Thread), Name, Host, Eui64, status badge (Pending/Connected/Offline/Rejected — màu riêng từng trạng thái), hành động **Accept / Reject**.
  - Empty state (`fallbackConditions`).
  - Accept → dialog đặt tên + chọn dataset → hub/API.
- **`WeaveApp.tsx`** — nếu cần route/tab (Network view hiện tại có thể đủ).

**Verify Batch 3:** login desktop → Network view → thấy device Pending → Accept → `Connected` + data channel chạy → disconnect → `Offline` → Reject → row `Rejected`, không tái xuất hiện.

---

## Batch 4 — Identity ECDSA TOFU + challenge-response (hardening)

> Schema đã chứa `Network.PublicKey` (nullable, Thread-only) từ Batch 2 — Batch 4 chỉ lấp đầy, không phá schema.

### Firmware
- **Keypair lúc factory** (boot): ECDSA **P-256** (ES256) — hardware crypto accelerator; private key vào **NVS encrypted** (eFuse nếu secure boot). `mbedtls` PK.
- **Handshake:** 1) backend gửi **nonce/challenge** (chống replay); 2) BR trả `Eui64 || public key || signature(nonce)`; 3) bị reject → nhận mã lỗi + backoff dài.

### Backend
- **Challenge correlation** — tái dùng `PendingFrameStore` match response với nonce.
- **`BrProvisioningService`** — verify signature (mbedtls ECDSA) trước khi tạo/duy trì record; lưu `PublicKey`.
- **Accept** → chốt `(Eui64, PublicKey)` bất biến.
- **Sau accept — reconnect:** BR ký client-assertion **JWT (ES256)**; backend verify theo `PublicKey` đã pin. **Không** lộ static token/secret. (Giữ pattern OAuth2 nhưng đổi sang ES256 — không tái dùng RSA/desktop OAuth2.)
- Verify lỗi quá N lần → tự `Rejected` (chống brute-force spoof).

**Verify Batch 4:** giả mạo Eui64 (thiết bị khác khai cùng Eui64) → bị chặn vì thiếu private key đúng; challenge replay cũ → loại; reconnect đúng → JWT ES256 verify pass.

---

## File-by-file tổng hợp

### Backend (`backend/src`)
| File | Thay đổi | Trạng thái |
|---|---|---|
| `Models/Enums.cs` | mới — `Protocol`, `NetworkStatus` | ✅ B1 |
| `Models/Network.cs` | mới — base entity; + nav 1:1 `BrThreadDataset? ThreadDataset` (B2). Class `Network`, bảng `Networks` (tên DbSet, không `ToTable`) | ✅ B1+B2 |
| `Models/BrThreadDataset.cs` | mới — 1:1 detail Thread, PK/FK = `NetworkId` (B2) | ✅ B2 |
| `Models/ZigbeeCoordinator.cs` | mới — skeleton | ⏳ deferred |
| `Dtos/NetworkDto.cs` | mới — DTO của entity chính `Network`; **root `Dtos/` + namespace `Namorix.Weave.Dtos`, ngoài BorderRouter** (vì là lớp chính, không phải BR-specific) | ✅ B2 |
| `Services/WeaveSecretProtector.cs` | mới — bọc `IDataProtectionProvider` (pattern `BcnSecretProtector`), purpose `Weave.ThreadDataset`; keyring `addon.DataDir/keys` (B2) | ✅ B2 |
| `Persistence/WeaveDbContext.cs` | mới — SQLite context riêng; DbSet `Networks`/`BrThreadDataset`, 1:1 cascade | ✅ B1+B2 |
| `Persistence/WeaveDbContextFactory.cs` | mới — design-time factory | ✅ B1 |
| `Migrations/` | `20260814022928_InitialCreate` — tạo cả 2 bảng `Networks` + `BrThreadDataset` (PK/FK `NetworkId`, cascade, unique Eui64 partial); **đã apply** | ✅ B2 |
| `Namorix.Weave.csproj` | + `Microsoft.EntityFrameworkCore.Sqlite` (và Design) + `Makaretu.Dns.Multicast` | ✅ B1 |
| `Services/BorderRouter/BrOptions.cs` | bỏ `Host`; thêm `MdnsServiceName` + `FramePort` :5150 | ✅ B1 |
| `BorderRouter/BrMdnsBrowser.cs` | mới — browse mDNS, dedupe, `BrFound/BrLost` | ✅ B1 |
| `BorderRouter/BrTcpClient.cs` | restore committed — connect-out + reconnect backoff | ✅ B1 |
| `BorderRouter/Dtos/BrDtos.cs` + `BrDtoMapper.cs` | DTO/mapper BR-specific (state/health/dataset/tables/joiners); `NetworkDto` đã tách ra `Dtos/` root | ✅ B1+B2 |
| `BorderRouter/BrCommandClient.cs` | refactor dùng `BrTcpClient`; mã lỗi reject (B4) | ✅ B1; reject-code ⏳ B4 |
| `BorderRouter/Frame/Commands.cs` | + command challenge/handshake/reject-code | ⏳ B4 |
| `Dtos/NetworkAcceptRequest.cs` | mới — `NetworkAcceptRequest` + `ThreadDatasetInput` (Payload Accept) | ✅ B2 |
| `Services/BorderRouter/BrProvisioningService.cs` | mới — handshake Eui64 → row `Pending`; `AcceptAsync`/`RejectAsync`/`MarkOfflineAsync` (+ `WeaveSecretProtector` ctor) | ✅ B1+B2 |
| `Services/BorderRouter/BrConnectionService.cs` | multi-BR (1 client/BR), poll/push gate theo Status; `AcceptAsync`/`RejectAsync` orchestration + `ApplyDatasetAsync` (Set*→StartThread); disconnect → `Offline` (`MarkOfflineAndPushAsync`) | ✅ B1+B2 |
| `Hubs/WeaveHub.cs` + `Constants` | rename từ `BrHub`; `NetworkList` event; `AcceptNetwork`/`RejectNetwork`; + `NetworkChanged` | ✅ B1+B2 |
| `Program.cs` | + `DataDirectory` (từ `NmxAddonConfig.DataDir`) + `AddDbContextFactory` + `Migrate()` + `AddDataProtection()` (keyring `data/keys`); DI `BrMdnsBrowser`, `BrProvisioningService` | ✅ B1+B2 |

### Firmware (`border-router-host`)
| File | Thay đổi | Trạng thái |
|---|---|---|
| `main/transport/frame_tcp.c` + `.h` | restore server — listen `CONFIG_BR_FRAME_TCP_PORT` :5150, accept-1, RX dispatch, state watchdog | ✅ B1 |
| `main/main.c` | restore mDNS advertise `_thread-border-router-frame._tcp` + `frame_tcp_init()` sau backhaul IPv4; bỏ block client-connect | ✅ B1 |
| `include/br_config.h` + `main/Kconfig.projbuild` | restore `TASK_NAME_TCP_ACCEPT/TCP_RX/STATE_WD` + `BR_FRAME_TCP_PORT` :5150; bỏ `BR_BACKEND_HOST/PORT` | ✅ B1 |
| `main/CMakeLists.txt` + `idf_component.yml` | thêm lại `espressif/mdns` | ✅ B1 |
| `main/openthread/ot_launch.c` (B4) | keypair factory + NVS encrypted | ⏳ B4 |
| `main/transport/frame_tcp.c` (B4) | handshake nonce + sign | ⏳ B4 |

### Frontend (`frontend/src`)
| File | Thay đổi | Trạng thái |
|---|---|---|
| `types/network.ts` | `NetworkStatus`, `Network` (mirror DTO) | ✅ B3 mục 1 |
| `signalr/constants.ts` | `NetworkList` + `NetworkChanged` | ✅ B3 mục 2 |
| `store/slices/networkSlice.ts` | + `devices` table + `setNetworkList`/`upsertNetwork` | ✅ B3 mục 3 |
| `store/selectors/networkSelectors.ts` | `selectNetworks` + `selectNetworkCountByStatus` | ✅ B3 mục 3 |
| `hooks/useNetworks.ts` | mới — SignalR feed | ✅ B3 mục 4 |
| `views/network/NetworkView.tsx` | list + detail: `BrProvisionPanel` (list) ↔ `ThreadNetworkView` (detail, `onBack`) | ✅ B3 |
| `views/network/BrProvisionPanel.tsx` | mới — `NmxGrid` cards + status badge + Accept/Reject dialog (REST) | ✅ B3 |

---

## Open decisions (cần user chốt)

1. **QR fingerprint lên case** lúc factory để Accept đối chiếu bằng mắt, hay **TOFU thuần** (tin connect đầu tiên trên LAN nội bộ)? → ảnh hưởng Batch 4.
2. **Dataset gán lúc Accept lấy từ đâu?** (a) admin nhập tay network settings → `ThreadDataset` + đẩy qua `SetPanId`/`SetChannel`/...; (b) copy từ dataset của BR đã `Connected`; (c) sinh dataset mới. → ảnh hưởng Batch 2/3 form.
3. **Multi-device concurrent** — nhiều BR `Connected` cùng lúc hay phase đầu chỉ 1 active + nhiều Pending/Offline/Rejected?

## Out of scope (ghi chú)
- **Zigbee** — chưa làm, chỉ dựng skeleton (enum + detail table + nav prop) để mở rộng sau; UI chỉ hiện protocol badge Thread. **ESPHome** — đã loại, không scaffold.
- **Multi-BR concurrent data channel** — refactor lớn, follow-up riêng.
- **Secure boot / eFuse toàn phần** — Batch 4 dùng NVS encrypted trước, eFuse khi bật secure boot.
- BR "Scan/Nhập tay" — đã loại khỏi thiết kế.
- Encryption key quản lý secret — weave giữ key riêng (`NetworkKeyEncrypted`/`ApiEncryptionKeyEncrypted` encrypt tầng app), nhất quán với quyết định "mỗi addon tự quản secret".

## Checklist
- [x] Phase 0: chốt hướng B (HA-style) — BR advertise mDNS + backend browse/connect-out
- [x] Batch 1: backend browse mDNS + connect-out (`BrMdnsBrowser`/`BrConnectionService`), firmware server + mDNS, handshake Eui64 → `Pending` (kèm DB slice tối thiểu)
- [x] Batch 1 verify: flash firmware → `avahi-browse` thấy BR → backend connect → row `Pending` trong DB (user xác nhận 2026-08-14)
- [x] Batch 2 (phần model + migration): `BrThreadDataset` model + 1:1 cascade + `WeaveSecretProtector` (DataProtection) + `NetworkDto` ở root `Dtos/` (`Namorix.Weave.Dtos`, ngoài BorderRouter) — class `Network`/`BrThreadDataset`, bảng `Networks`/`BrThreadDataset` (tên DbSet, không `ToTable`, bỏ hẳn `BrNetwork`); migration `20260814022928_InitialCreate` (cả 2 bảng) **đã apply** (verified 2026-08-14)
- [x] Batch 2 (phần còn lại): accept/reject/blacklist (`Status=Rejected` + unique Eui64 index) + Offline + SignalR events (registry-changed) — `BrProvisioningService.AcceptAsync`/`RejectAsync`/`MarkOfflineAsync` + `WeaveHub.AcceptNetwork`/`RejectNetwork` + `ApplyDatasetAsync` (SetPanId→StartThread khi BR online) + `NetworkChanged` (`ZigbeeCoordinator` deferred)
- [x] Batch 3 (mục 1–4): types `Network`/`NetworkStatus` + SignalR `NetworkList`/`NetworkChanged` + store `devices`/selectors + `useNetworks` hook (tsc sạch)
- [x] Batch 3 (còn lại): REST `NetworkController` (`POST /api/networks/{id}/accept|reject`) + `network.controller.ts` (thay hub invoke) + `BrProvisionPanel` (`NmxGrid` + Accept/Reject dialog) + wire `NetworkView` list/`ThreadNetworkView` detail + i18n `en.json` + version bump **0.4.2 → 0.5.0** (vi.json deferred — user bỏ qua)
- [ ] Batch 4: ECDSA P-256 keypair + challenge-response + pin `(Eui64, PublicKey)` + JWT ES256 reconnect
- [ ] Verify: spoof Eui64 bị chặn; reject → backoff dài, không spam Pending; disconnect → `Offline`; accept → data channel chạy
