<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { showFailToast } from 'vant'
import { useAuthStore } from '@/stores/auth'

const route = useRoute()
const router = useRouter()
const auth = useAuthStore()
const error = ref('')

onMounted(async () => {
  const code = route.query.code as string | undefined
  const state = route.query.state as string | undefined
  if (!code || !state) {
    error.value = '缺少 OAuth 参数'
    return
  }
  try {
    await auth.handleCallback(code, state)
    const redirect = (route.query.redirect as string) || '/conversations'
    await router.replace(redirect)
  } catch (e) {
    error.value = e instanceof Error ? e.message : String(e)
    showFailToast(error.value)
  }
})
</script>

<template>
  <div class="callback">
    <p v-if="!error">登录中…</p>
    <p v-else>{{ error }}</p>
  </div>
</template>

<style scoped>
.callback {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #666;
}
</style>
