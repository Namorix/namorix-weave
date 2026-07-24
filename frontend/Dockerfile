FROM node:22-alpine AS build
RUN corepack enable && corepack prepare pnpm@latest --activate

WORKDIR /app
COPY . /app/namorix-weave/frontend/

COPY --from=namorix frontend/packages/core /app/namorix/frontend/packages/core/
COPY --from=namorix frontend/tsconfig.base.json /app/namorix/frontend/tsconfig.base.json
COPY --from=namorix frontend/packages/styles /app/namorix/frontend/packages/styles/
COPY --from=namorix frontend/packages/ui /app/namorix/frontend/packages/ui/

WORKDIR /app/namorix/frontend/packages/core
RUN corepack prepare pnpm@latest --activate
RUN printf 'allowBuilds:\n  esbuild: true\n' > pnpm-workspace.yaml
RUN pnpm install
RUN pnpm build

WORKDIR /app/namorix-weave/frontend
RUN printf 'allowBuilds:\n  esbuild: true\n  "@parcel/watcher": true\n' > pnpm-workspace.yaml
RUN CI=true pnpm install
RUN pnpm build

FROM node:22-alpine
WORKDIR /app
COPY --from=build /app/namorix-weave/frontend/dist ./dist
RUN npm install -g serve
EXPOSE 5100
CMD ["serve", "dist", "-l", "5100", "--cors"]
