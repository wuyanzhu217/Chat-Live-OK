<script setup lang="ts">
import { Button, CellGroup, Cell } from 'vant'
</script>

<template>
  <div class="home">
    <header class="home__header">
      <h1>Chat-Live-OK</h1>
      <p>Vue Web 脚手架 — P1 起实现登录与聊天</p>
    </header>
    <CellGroup inset title="开发入口">
      <Cell title="直播开播 (占位)" is-link to="/live/broadcast" />
      <Cell title="直播观看 (占位)" is-link to="/live/watch/demo" />
    </CellGroup>
    <p class="home__hint">
      先运行 <code>make dev-up</code> 启动 SRS 等后端服务。
    </p>
  </div>
</template>

<style scoped>
.home {
  padding: 16px;
  padding-bottom: calc(16px + env(safe-area-inset-bottom));
}
.home__header h1 {
  margin: 0 0 8px;
  font-size: 22px;
}
.home__header p {
  margin: 0 0 16px;
  color: #666;
  font-size: 14px;
}
.home__hint {
  margin-top: 24px;
  font-size: 13px;
  color: #999;
}
</style>
