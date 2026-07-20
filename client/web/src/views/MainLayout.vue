<script setup lang="ts">
import { RouterView } from 'vue-router'
import { Tabbar, TabbarItem } from 'vant'
import { useRoute } from 'vue-router'

const route = useRoute()

const active = () => {
  if (route.path.startsWith('/friends')) return 'friends'
  if (route.path.startsWith('/live')) return 'live'
  if (route.path.startsWith('/me')) return 'me'
  return 'conversations'
}
</script>

<template>
  <div class="layout">
    <RouterView />
    <Tabbar :model-value="active()" route>
      <TabbarItem replace to="/conversations" icon="chat-o" name="conversations">消息</TabbarItem>
      <TabbarItem replace to="/friends" icon="friends-o" name="friends">好友</TabbarItem>
      <TabbarItem replace to="/live" icon="video-o" name="live">直播</TabbarItem>
      <TabbarItem replace to="/me" icon="user-o" name="me">我的</TabbarItem>
    </Tabbar>
  </div>
</template>

<style scoped>
.layout {
  min-height: 100vh;
  padding-bottom: 50px;
  box-sizing: border-box;
}
</style>
