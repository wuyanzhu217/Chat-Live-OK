<script setup lang="ts">
import { RouterView } from 'vue-router'
import CallOverlay from '@/components/CallOverlay.vue'
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
