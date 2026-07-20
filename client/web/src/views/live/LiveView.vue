<script setup lang="ts">
import { useRoute, useRouter } from 'vue-router'
import { NavBar, Tab, Tabs } from 'vant'
import LiveSquareView from '@/views/live/LiveSquareView.vue'
import LiveStudioView from '@/views/live/LiveStudioView.vue'

const route = useRoute()
const router = useRouter()

const activeTab = () => (route.query.tab === 'studio' ? 'studio' : 'square')

function onTabChange(name: string | number): void {
  void router.replace({ path: '/live', query: name === 'studio' ? { tab: 'studio' } : {} })
}
</script>

<template>
  <div class="page">
    <NavBar title="直播" fixed placeholder />
    <Tabs :active="activeTab()" sticky offset-top="46" @change="onTabChange">
      <Tab title="广场" name="square">
        <LiveSquareView />
      </Tab>
      <Tab title="开播" name="studio">
        <LiveStudioView />
      </Tab>
    </Tabs>
  </div>
</template>

<style scoped>
.page {
  min-height: 100vh;
  background: #f7f8fa;
}
</style>
