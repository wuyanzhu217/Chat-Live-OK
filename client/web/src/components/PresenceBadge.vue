<script setup lang="ts">
import type { Presence } from '@/types/friend'

defineProps<{ presence?: Presence }>()
</script>

<template>
  <span
    class="presence"
    :class="{
      'presence--online': presence === 'online',
      'presence--offline': !presence || presence === 'offline',
    }"
  />
</template>

<style scoped>
.presence {
  position: absolute;
  right: 0;
  bottom: 0;
  width: 10px;
  height: 10px;
  border-radius: 50%;
  border: 2px solid #fff;
}
.presence--online {
  background: #07c160;
}
.presence--offline {
  background: #ccc;
}
</style>
