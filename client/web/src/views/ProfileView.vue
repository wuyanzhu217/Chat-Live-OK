<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import {
  Button,
  Cell,
  CellGroup,
  Field,
  Icon,
  NavBar,
  showConfirmDialog,
  showToast,
} from 'vant'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useRealtimeStore } from '@/stores/realtime'
import UserAvatar from '@/components/UserAvatar.vue'
import { compressImageForUpload } from '@/utils/imageUpload'
import { ApiError } from '@/types/api'

const auth = useAuthStore()
const realtime = useRealtimeStore()
const router = useRouter()

const nickname = ref(auth.user?.nickname ?? '')
const savingNickname = ref(false)
const uploadingAvatar = ref(false)
const fileInput = ref<HTMLInputElement | null>(null)

const nicknameDirty = computed(() => {
  const trimmed = nickname.value.trim()
  return trimmed.length > 0 && trimmed !== (auth.user?.nickname ?? '')
})

watch(
  () => auth.user?.nickname,
  (v) => {
    if (v !== undefined) nickname.value = v
  },
)

function openAvatarPicker(): void {
  if (uploadingAvatar.value) return
  fileInput.value?.click()
}

async function onAvatarSelected(ev: Event): Promise<void> {
  const input = ev.target as HTMLInputElement
  const file = input.files?.[0]
  input.value = ''
  if (!file) return

  uploadingAvatar.value = true
  try {
    const compressed = await compressImageForUpload(file)
    await auth.changeAvatar(compressed)
    showToast('头像已更新')
  } catch (err) {
    const msg =
      err instanceof ApiError
        ? err.message
        : err instanceof Error
          ? err.message
          : '上传头像失败'
    showToast(msg)
  } finally {
    uploadingAvatar.value = false
  }
}

async function onSaveNickname(): Promise<void> {
  const trimmed = nickname.value.trim()
  if (!trimmed) {
    showToast('昵称不能为空')
    return
  }
  if (trimmed.length > 64) {
    showToast('昵称最多 64 个字符')
    return
  }
  if (trimmed === auth.user?.nickname) {
    showToast('昵称未更改')
    return
  }

  savingNickname.value = true
  try {
    await auth.updateProfile(trimmed)
    showToast('昵称已保存')
  } catch (err) {
    const msg =
      err instanceof ApiError
        ? err.message
        : err instanceof Error
          ? err.message
          : '保存失败'
    showToast(msg)
  } finally {
    savingNickname.value = false
  }
}

async function onSwitchAccount(): Promise<void> {
  try {
    await showConfirmDialog({
      title: '切换账号',
      message: '将退出当前账号并重新登录，可使用其他账号。',
    })
  } catch {
    return
  }
  await auth.switchAccount()
}

async function onLogout(): Promise<void> {
  try {
    await showConfirmDialog({
      title: '退出登录',
      message: '确定退出当前账号？',
    })
  } catch {
    return
  }
  await auth.logout({ federated: true })
  if (!auth.isAuthenticated) {
    void router.replace('/login')
  }
}
</script>

<template>
  <div class="page">
    <NavBar title="我的" fixed placeholder />

    <section class="hero">
      <button
        type="button"
        class="hero__avatar"
        :disabled="uploadingAvatar"
        aria-label="更换头像"
        @click="openAvatarPicker"
      >
        <UserAvatar
          v-if="auth.user"
          :name="auth.user.nickname"
          :avatar-url="auth.user.avatar_url"
          :user-id="auth.user.id"
          :size="80"
        />
        <span class="hero__camera" aria-hidden="true">
          <Icon :name="uploadingAvatar ? 'clock-o' : 'photograph'" size="14" />
        </span>
      </button>
      <input
        ref="fileInput"
        type="file"
        accept="image/*"
        class="hero__file"
        @change="onAvatarSelected"
      />

      <template v-if="auth.user">
        <h1 class="hero__name">{{ auth.user.nickname }}</h1>
        <div
          class="hero__status"
          :class="realtime.connected ? 'hero__status--on' : 'hero__status--off'"
        >
          <span class="hero__status-dot" />
          {{ realtime.connected ? '实时已连接' : '实时未连接' }}
        </div>
      </template>
    </section>

    <CellGroup inset class="block" title="个人资料">
      <Field
        v-model="nickname"
        label="昵称"
        maxlength="64"
        placeholder="聊天、直播里显示的名字"
        clearable
      >
        <template #button>
          <Button
            size="small"
            type="primary"
            round
            :loading="savingNickname"
            :disabled="!nicknameDirty"
            @click="onSaveNickname"
          >
            保存
          </Button>
        </template>
      </Field>
      <Cell
        title="用户名"
        :value="auth.user ? `@${auth.user.username}` : '—'"
        label="登录账号，不可修改"
      />
    </CellGroup>

    <CellGroup inset class="block">
      <Cell
        title="我的直播"
        label="开播与直播间管理"
        is-link
        to="/live?tab=studio"
      >
        <template #icon>
          <Icon name="video-o" class="menu-icon" />
        </template>
      </Cell>
      <Cell title="直播广场" label="发现正在直播的房间" is-link to="/live">
        <template #icon>
          <Icon name="fire-o" class="menu-icon" />
        </template>
      </Cell>
    </CellGroup>

    <div class="actions">
      <Button block round plain hairline type="primary" @click="onSwitchAccount">
        切换账号
      </Button>
      <Button block round plain hairline type="danger" @click="onLogout">退出登录</Button>
    </div>
  </div>
</template>

<style scoped>
.page {
  min-height: 100%;
  background: #f7f8fa;
}

.hero {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  padding: 28px 20px 24px;
  margin: 0 12px 12px;
  border-radius: 16px;
  background: #fff;
}

.hero__avatar {
  position: relative;
  border: none;
  background: transparent;
  padding: 0;
  cursor: pointer;
  line-height: 0;
}

.hero__avatar:disabled {
  opacity: 0.75;
  cursor: wait;
}

.hero__camera {
  position: absolute;
  right: 0;
  bottom: 0;
  width: 28px;
  height: 28px;
  border-radius: 50%;
  background: #323233;
  color: #fff;
  display: flex;
  align-items: center;
  justify-content: center;
  border: 2px solid #fff;
}

.hero__file {
  display: none;
}

.hero__name {
  margin: 14px 0 0;
  font-size: 20px;
  font-weight: 600;
  line-height: 1.3;
  color: #323233;
  max-width: 100%;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.hero__status {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  margin-top: 12px;
  padding: 4px 10px;
  border-radius: 999px;
  font-size: 12px;
  line-height: 1.4;
  background: #f2f3f5;
  color: #969799;
}

.hero__status--on {
  background: #e8f8ef;
  color: #07c160;
}

.hero__status--off {
  background: #f2f3f5;
  color: #969799;
}

.hero__status-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: currentColor;
  flex-shrink: 0;
}

.block {
  margin-bottom: 12px;
}

.menu-icon {
  margin-right: 12px;
  font-size: 20px;
  color: #646566;
  align-self: center;
}

.actions {
  display: flex;
  flex-direction: column;
  gap: 10px;
  padding: 8px 16px 28px;
}
</style>
