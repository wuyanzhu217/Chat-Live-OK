<script setup lang="ts">
import { onMounted } from 'vue'
import { RouterView } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import CallOverlay from '@/components/CallOverlay.vue'

const auth = useAuthStore()

onMounted(() => {
  void auth.bootstrap()
})
</script>

<template>
  <RouterView />
  <CallOverlay />
</template>

<style>
html,
body,
#app {
  margin: 0;
  min-height: 100%;
  background: #f7f8fa;
}
</style>
