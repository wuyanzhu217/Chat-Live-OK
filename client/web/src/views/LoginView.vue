<script setup lang="ts">
import { Button } from 'vant'
import { useAuthStore } from '@/stores/auth'

const auth = useAuthStore()
</script>

<template>
  <div class="login">
    <div class="login__brand">
      <h1>Chat-Live-OK</h1>
      <p>即时通讯 · 直播演示</p>
    </div>
    <Button type="primary" block round size="large" @click="auth.login()">使用 Keycloak 登录</Button>
    <p class="login__hint">测试账号：alice / alice_dev，bob / bob_dev</p>
  </div>
</template>

<style scoped>
.login {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  justify-content: center;
  padding: 32px 24px;
  box-sizing: border-box;
  background: linear-gradient(160deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%);
  color: #fff;
}
.login__brand h1 {
  margin: 0 0 8px;
  font-size: 28px;
}
.login__brand p {
  margin: 0 0 40px;
  opacity: 0.85;
}
.login__hint {
  margin-top: 24px;
  font-size: 13px;
  opacity: 0.7;
  text-align: center;
}
</style>
