<script setup lang="ts">
import { onMounted } from 'vue'
import { RouterView } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const auth = useAuthStore()

onMounted(() => {
  void auth.bootstrap()
})
</script>

<template>
  <RouterView />
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
