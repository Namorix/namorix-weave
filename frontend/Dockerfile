FROM node:22-alpine AS build
RUN corepack enable && corepack prepare pnpm@latest --activate

WORKDIR /app
COPY . /app/namorix-thread/frontend/
COPY --from=namorix frontend/packages/core /app/namorix/frontend/packages/core/

WORKDIR /app/namorix-thread/frontend
RUN CI=true pnpm install
RUN pnpm build

FROM node:22-alpine
WORKDIR /app
COPY --from=build /app/namorix-thread/frontend/dist ./dist
RUN npm install -g serve
EXPOSE 5180
CMD ["serve", "dist", "-l", "5180", "--cors"]
