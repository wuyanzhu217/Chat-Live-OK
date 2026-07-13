<script setup lang="ts">
import { computed } from 'vue'

const props = withDefaults(
  defineProps<{
    name: string
    avatarUrl?: string | null
    userId?: string
    size?: number
  }>(),
  { size: 40 },
)

const initials = computed(() => {
  const n = props.name?.trim() || '?'
  return n.slice(0, 1).toUpperCase()
})

const style = computed(() => ({
  width: `${props.size}px`,
  height: `${props.size}px`,
  fontSize: `${Math.round(props.size * 0.4)}px`,
}))
</script>

<template>
  <div class="avatar" :style="style">
    <img v-if="avatarUrl" :src="avatarUrl" :alt="name" class="avatar__img" />
    <span v-else class="avatar__text">{{ initials }}</span>
  </div>
</template>

<style scoped>
.avatar {
  border-radius: 50%;
  overflow: hidden;
  background: linear-gradient(135deg, #667eea, #764ba2);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}
.avatar__img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}
.avatar__text {
  color: #fff;
  font-weight: 600;
}
</style>
