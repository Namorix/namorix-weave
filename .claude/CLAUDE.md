# CLAUDE.md

This file provides guidance to AI agents when working with this repository (Namorix Weave addon).

## REQUIRED: Read Memory Bank on Start

Every session must read:
- `.claude/memory/MEMORY.md` — start here (index)
- `.claude/memory/activeContext.md` — current work focus
- `.claude/memory/progress.md` — version history

## Project Overview

**Namorix Weave** is a **Namorix addon** — a self-contained web app the desktop shell mounts as a Module Federation remote. It lets users connect and monitor smart home devices across a **Thread mesh**. The desktop stays the orchestrator (auth, addon lifecycle, event bus); Weave is a guest that trusts Desktop-issued sessions and only verifies via API.

## Architecture Principles

| Principle | Explanation |
|-----------|-------------|
| Addon is a guest of Namorix Desktop | Desktop is the only auth server. Weave trusts its OAuth2 session and only verifies via `Namorix.Core` OAuth client. |
| gRPC addon channel | Backend ↔ Namorix server via `AddonChannelClient` / `AddonHostedServiceBase` (`AddonChannelClient` from `Namorix.Core`). |
| SignalR | Frontend ↔ backend realtime events. Client dep is present; wire it up per feature. |
| Module Federation remote | Frontend exposes `./Addon`; `react`, `react-dom`, `i18next`, `react-i18next` shared as singletons. |
| Shared packages, never duplicated | Consume `@namorix/core`, `@namorix/ui`, `@namorix/styles` from the sibling `namorix` repo via `link:` — never vendor copies. |
| Thread logic lives in a sibling repo | The Thread/Matter stack is `namorix-thread`; Weave orchestrates and monitors. |

## Tech Stack

- **Frontend:** Vite 8 + React 19 + TypeScript, Module Federation, Redux Toolkit, i18next, SignalR client, SCSS via `@namorix/styles`
- **Backend:** ASP.NET Core 10 (C#), gRPC addon channel, OAuth2 client, JWT
- **Shared packages (sibling repo):** `@namorix/core`, `@namorix/ui`, `@namorix/styles` at `../../namorix/frontend/packages/*`
- **Backend core (sibling repo):** `Namorix.Core` at `../../../namorix/backend/src/Namorix.Core`

## Package Boundaries

| Package | Can Import |
|---------|------------|
| `frontend/` | `@namorix/core`, `@namorix/ui`, `@namorix/styles`, React ecosystem |
| `backend/` | ASP.NET Core, `Namorix.Core`, gRPC, JWT |

Never reach into desktop-internal modules; always go through the shared packages above.

## Key Interfaces (`@namorix/core` addon-facing)

```typescript
// Mount the addon (frontend/src/mount.tsx)
import { createMount } from "@namorix/core"
export const mount = createMount(WeaveApp)

// i18n merge (frontend/src/i18n/index.ts)
import { ensureI18n } from "@namorix/core"
ensureI18n({ en, vi })

// Runtime context
useAddonMode(): string     // "addon" | "standalone" | ...
useIsStandalone(): boolean

// Shared UI
import { NmxRail, NmxRailList, NmxRailContent } from "@namorix/ui"
```

## Development

```bash
# Frontend (from frontend/)
cd frontend && pnpm dev           # Vite dev server — port $ADDON_FRONTEND_PORT (default 5100)
cd frontend && pnpm build         # Production build (vite build)

# Backend (from backend/)
cd backend && make run            # dotnet run — port $ADDON_BACKEND_PORT (default 5101)
cd backend && make watch          # dotnet watch run
cd backend && make build          # dotnet build Namorix.Weave.sln

# Docker (frontend context — build needs sibling repo as additional context)
cd frontend && pnpm docker:prod   # docker compose down && up --build
```

`frontend/.env`: `ADDON_FRONTEND_PORT=5100`, `ADDON_BACKEND_PORT=5101`, `ADDON_HOST=http://localhost`. Dev server proxies `/.well-known` to the backend.

## Current Status (v0.3.0)

Scaffold + Network monitoring UI now fed by live Border-Router data over SignalR.
- **Backend:** `WeaveService` connects over gRPC and pings Namorix with a `widget-event` (`{"event":"ready"}`). The BR integration stack (frame protocol, commands, parsers, `BrConnectionService`) polls the ESP32-S3 border-router over TCP 5000 and pushes live status/dataset/table snapshots to the frontend via SignalR at `/hubs/weave`. Event names are shared constants in `Constants/WeaveSignalR.cs` (`border-router:*`).
- **Frontend:** `WeaveApp` wraps the shell in a Redux store and defaults to the Network tab. A core singleton is built from the `@namorix/core` factory pattern in `src/config/coreConfig.ts` (`createNmxCore({ hubsPath: "/hubs/weave" })` + signalr hooks), imported side-effect in `bootstrap.tsx`. `NetworkView` shows an OTBR/Thread dashboard — `NmxToolbar` sub-tabs (Overview, Dataset, Mesh, Joiner) with connection status, active dataset, and router/child/joiner tables. Live data flows through `src/signalr/` (`WeaveSignalREvents` constants + `useSignalR`/`useSignalREvent`/`useSignalRStatus` hooks) into `hooks/useOtbrData.ts`, which dispatches into `networkSlice`. Dashboard/Devices/Settings tabs are still placeholders.
- Real Thread/device monitoring (multi-device beyond the border-router) is not implemented yet; port 5100 is the addon's web entry (`addon.json`).

---

## Rule 0: Code Suggestion Only (Suggestion Mode)

AI agents in this project operate in suggestion mode only:

- **DO:** Suggest code snippets, point out issues, ask clarifying questions before implementing
- **DON'T:** Write code without being explicitly requested by the user
- **DON'T:** Refactor, rewrite, or "improve" code without user approval

When the user asks for something that requires code changes:
1. First understand the current state (read relevant files)
2. Present a clear plan or approach (possibly with code snippets)
3. Wait for user confirmation before implementing
4. After user approval, write the actual code

**Rationale:** Prevents unwanted changes; the user keeps full control over the codebase.

---

## Rule 1: TypeScript Configuration

Mirror `frontend/tsconfig.json`; do not loosen these flags for new code:

```json
{
  "compilerOptions": {
    "baseUrl": ".",
    "target": "ES2023",
    "lib": ["ES2023", "DOM"],
    "module": "ESNext",
    "moduleResolution": "bundler",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "noImplicitOverride": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "verbatimModuleSyntax": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "jsx": "react-jsx",
    "allowImportingTsExtensions": true,
    "noEmit": true,
    "paths": { "@namorix/core": ["../../namorix/frontend/packages/core/src/index.ts"] }
  },
  "include": ["src"]
}
```

Rationale: bundler resolution for Vite, strict mode, `verbatimModuleSyntax` enforces type-only imports.

---

## Rule 2: REQUIRED — Scan Codebase Before Suggesting

Before suggesting code or solutions, you MUST scan the relevant codebase first:

1. Read the actual files involved in the task
2. Check git diff to see what changed recently
3. Verify existing patterns and conventions
4. Only then provide suggestions based on what you found

Never suggest code without understanding the current state. This prevents duplicate implementations, conflicts, and ignoring recent changes.

---

## Rule 3: Package Boundary (ESLint)

```
frontend — allowed to import:
- @namorix/core
- @namorix/ui
- @namorix/styles
- React ecosystem

backend (ASP.NET Core) — allowed to import:
- ASP.NET Core ecosystem
- Namorix.Core (sibling repo)
```

- Enforce via ESLint/import plugin to ban cross-package boundaries
- Rationale: prevent circular dependencies and leaking server implementation into browser code

---

## Rule 4: Import/Export Pattern

```typescript
// ✅ Correct — barrel export in index.ts
export { getSession } from "./auth"
export { connectEvents } from "./events"

// ❌ Wrong — re-export not through barrel
import { getSession } from "@namorix/core/auth"
```

## Component File Structure
- Named export for React components (NOT default export)
- Named export for utilities/helpers

## Import Order (separate groups with one blank line)
```
1. React / framework
2. @namorix/core / @namorix/ui / @namorix/styles
3. Internal imports (./, ../)
4. Types (type imports only)
```

---

## Rule 5: React Component Rules

```typescript
// ✅ Correct — named export
const DashboardView: React.FC = () => {
  const addonMode = useAddonMode()
  return <h1>Dashboard: {addonMode}</h1>
}

// ❌ Wrong — default export
export default function DashboardView() {}
```

## File Naming
- **PascalCase** for component files: `DashboardView.tsx`
- **camelCase** for non-component files: `useWeaveDevices.ts`

## Hooks Naming
- Pattern: `use{Resource}` or `use{Action}`
- Examples: `useAddonMode`, `useWeaveNetwork`, `useAppDispatch`

## Store (Redux Toolkit) Pattern
- Slice file: `store/slices/{name}Slice.ts`
- Selector file: `store/selectors/{name}Selectors.ts`
- State normalized: `byId: Record<Id, Data>` + `order: Id[]`
- Actions via `useAppDispatch()` + action creators (stable references)
- `useAppSelector` defaults to `shallowEqual`

---

## Rule 6: Using UI Primitives (`@namorix/ui`)

Weave consumes `@namorix/ui`; it does **not** define new primitives. New shared primitives belong in `@namorix/ui` in the sibling `namorix` repo.

## Conventions when using primitives
| Element | Rule | Example |
|---------|------|---------|
| Component | `Nmx` prefix + PascalCase | `NmxRail`, `NmxButton`, `NmxIconFontSymbol` |
| Props interface | `[ComponentName]Props` | `NmxRailItemData<WeaveTab>` |
| Page/view component | PascalCase, no `Nmx` prefix | `DashboardView`, `WeaveApp` |
| CSS class | `nmx-kebab-case` BEM | `nmx-rail`, `nmx-tab--active` |

## Component Rules
- Functional components only (no class components)
- Props must have explicit TypeScript interface
- No inline styles — styling via SCSS module + `@namorix/styles`
- No hardcoded colors or spacing — use `--nmx-*` CSS variables from `@namorix/styles`
- Composite views: parent decides, children cascade via CSS (no duplicated `size` props)
- `shouldRender` instead of conditional ternary for show/hide

## SCSS Tokens from `@namorix/styles`
```scss
@use "@namorix/styles";

.weave-card {
  padding: var(--nmx-spacing-2) var(--nmx-spacing-4);
  border-radius: var(--nmx-radius-md);
  background-color: var(--nmx-color-surface-low);
  color: var(--nmx-color-on-surface);
  font-family: var(--nmx-font-sans);
}
```

## Tonal Elevation (Material Design 3)
- Use surface tone stack instead of border/shadow to separate elements:
  - `--nmx-color-surface-lowest` — inputs, textareas
  - `--nmx-color-surface-low` — cards, panels
  - `--nmx-color-surface` — main shell background
  - `--nmx-color-surface-mid` — highlighted blocks, active tab bg
  - `--nmx-color-surface-high` — hover states
  - `--nmx-color-surface-highest` — strong emphasis, active chip
- **No `border`** to separate — use two adjacent surface tones
- **No `box-shadow`** for elevation — only for real overlays (modal, dropdown, tooltip)

---

## Rule 7: Error Handling

## Frontend — throw ApiError on non-success response
```typescript
import { ApiError } from "@namorix/core"

if (!data.success) {
  throw ApiError.fromResponse(data)
}
```

## Page Component — use formatApiError for centralized formatting
```typescript
import { formatApiError } from "@namorix/core"

catch (err: unknown) {
  const message = formatApiError(t, err) ?? t("weave.errors.generic")
  setAlert({ message, variant: "error" })
}
```

## Client-side Validation — use ValidationRunner
```typescript
import { validate, ValidationFields as F } from "@namorix/core"

const error = validate(t)
  .required(F.USERNAME, username)
  .minLength(F.PASSWORD, password, 6)
  .first()
if (error) { setAlert({ message: error, variant: "error" }); return }
```

## Try/Catch
- Backend: try/catch in handler, log error then return 500
- Frontend: try/catch in event handler or component, display toast/notification

---

## Rule 8: Git Conventions

## Branch Naming
```
feature/{short-description}   # feature/weave-network-view
fix/{short-description}       # fix/addon-channel-reconnect
```
(No milestone number — this addon has no M-milestones.)

## Commit Message Format
```
{type}({scope}): {description}
```

### Types
- `feat`: new feature
- `fix`: bug fix
- `refactor`: behavior-preserving change
- `docs`: documentation
- `chore`: build, config, deps

### Scopes
- `frontend`: frontend/
- `backend`: backend/
- `addon`: addon.json
- `docker`: Dockerfiles, compose

### Examples
```
feat(frontend): add network view device list
fix(backend): handle addon channel reconnect
chore(docker): install shared packages as workspace
```

## PR Title
Same format as commit. Body describes WHAT and WHY, not HOW.

---

## Rule 9: File & Folder Structure

```
addon.json                 # Namorix addon manifest (id, ports, image, min versions)
assets/icon.svg

backend/
├── Makefile
├── Namorix.Weave.sln
└── src/
    ├── Program.cs         # DI: OAuth2 client, AddonChannelClient, WeaveService
    ├── appsettings.json
    └── Services/
        └── WeaveService.cs   # gRPC addon channel handler (AddonHostedServiceBase)
    # future: Controllers/, Models/, OAuth config

frontend/
├── package.json
├── vite.config.ts         # Module Federation remote "addon_weave", dev proxy / .well-known
├── tsconfig.json
├── pnpm-workspace.yaml
├── Dockerfile             # multi-stage; installs shared packages as workspace
├── docker-compose.yml
├── .env                   # ADDON_* ports
├── index.html
└── src/
    ├── bootstrap.tsx      # entry: awaits mount(root)
    ├── mount.tsx          # createMount(WeaveApp)
    ├── WeaveApp.tsx       # NmxRail shell with tabs
    ├── main.scss          # @forward "@namorix/styles"
    ├── i18n/
    │   ├── index.ts       # ensureI18n({ en, vi })
    │   └── locales/
    │       ├── en.json
    │       └── vi.json
    # future: views/ (DashboardView, DevicesView, NetworkView, SettingsView),
    #         store/, components/, controllers/
```

---

## Rule 10: Naming Conventions

```typescript
// Strings: double quotes
const ADDON_ID = "namorix-weave"
const errorMessage = "Device offline"

// Variables & Functions: camelCase
const weaveDevices = getWeaveDevices()
function buildNetworkGraph() {}

// Types & Interfaces: PascalCase
interface WeaveDevice { id: string; role: "leader" | "router" | "child" }
type WeaveTab = "dashboard" | "devices" | "network" | "settings"

// Constants: UPPER_SNAKE_CASE
const REFRESH_INTERVAL_MS = 30_000
const WIDGET_EVENT_READY = "widget-event"

// React components: PascalCase; Nmx prefix only for shared @namorix/ui components
// DashboardView, DevicesView (page/view components — no Nmx prefix)

// API responses / DTOs: camelCase
// gRPC message Type: colon/kebab namespaced strings — "widget-event", "weave:device-updated"

// CSS classes: nmx-kebab-case BEM — .nmx-rail, .weave-card
// CSS theme variables: --nmx-{component}-{property} — --nmx-card-bg
```

---

## Rule 11: C# Backend & Firmware C Naming Conventions

Quy tắc đặt tên **thống nhất** cho code C# (backend này) và firmware C (sibling `namorix-thread/firmware/br-host`). C# và C giữ case đúng ngôn ngữ, nhưng chung **nguyên tắc**: tên có nghĩa, nhất quán, không viết tắt tuỳ tiện, file trùng tên type/đơn vị.

| Khái niệm | C# backend | Firmware C |
|---|---|---|
| Type / class / struct / enum | `PascalCase` | `snake_case_t` |
| Method / function | `PascalCase` | `{module}_snake_case` |
| Biến local / tham số | `camelCase` | `snake_case` |
| Field private | `_camelCase` | `s_` (static) / `m_` (module) |
| Hằng số / enum value | `PascalCase` | `UPPER_SNAKE_CASE` |
| File | `PascalCase.cs` | `snake_case.c/.h` |
| Namespace / include guard | dotted PascalCase | `UPPER_SNAKE_H` |

Ví dụ C#: type `BrCommandClient`, method `RequestAsync`, field `_pendingFrames`, enum `BrRole`.
Ví dụ C: type `frame_t`, hàm `br_transport_send`, static `s_pending_ip_frame_id`, macro `CMD_STATE`.

---

## Meta Rules

## Skip LICENSE File
Do not read file `LICENSE` — it's license text, not needed for development tasks.

## Version Notation
Use Semantic Versioning `MAJOR.MINOR.PATCH` (no leading `v`). Keep `addon.json` `version` in sync with releases.

## Giao Tiếp Bằng Tiếng Việt
Luôn giao tiếp với người dùng bằng tiếng Việt. Tất cả phản hồi, giải thích, câu hỏi đều dùng tiếng Việt.

## Confirm Completion Before Moving On

IF người dùng chưa nói "Xong" / "Ok" / "Done":
- Chỉ xử lý đúng vấn đề đang bàn, không gợi ý thêm.

IF người dùng nói "Xong" / "Ok" / "Done":
- Phải dùng tool đọc lại toàn bộ file liên quan — không dựa vào memory.
- Chỉ sau khi đọc xong mới được báo cáo trạng thái hoặc suggest bước tiếp.
- Không được claim còn lỗi nếu chưa thật sự đọc file bằng tool.
- Nếu không có tool access → nói rõ "chưa đọc được file, bạn paste lên đi".
