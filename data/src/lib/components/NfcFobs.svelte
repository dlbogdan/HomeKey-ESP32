<script lang="ts">
  import { getNfcFobs, saveNfcFobs, addNfcFob, deleteNfcFob } from '$lib/services/api';
  import { isApiError } from '$lib/utils/type-guards';
  import type { NfcFobConfig } from '$lib/types/api';
  import ws, { type WebSocketEvent } from '$lib/services/ws.js';

  let { error }: { error?: string | null } = $props();

  let activeTab = $state<'config' | 'fobs'>('config');

  // svelte-ignore state_referenced_locally
  let nfcFobConfig = $state<NfcFobConfig>($state.snapshot({ enabled: false, fobs: [] }));

  let newUid = $state('');
  let newLabel = $state('');
  let newAtqa = $state('');
  let newSak = $state('');
  let loading = $state(true);
  let addError = $state('');

  // Scan mode: when active, the component will listen for NFC tag events via WebSocket
  let scanMode = $state(false);
  let scanStatus = $state('');

  // Handle incoming WebSocket events for NFC tag scans
  function handleNfcTagEvent(evt: WebSocketEvent<any>) {
    if (evt.type === 'message' && evt.data?.type === 'nfc_tag') {
      const data = evt.data;
      if (data?.uid && scanMode) {
        // Convert UID bytes to hex string
        const uidHex = data.uid.map((b: number) => b.toString(16).toUpperCase().padStart(2, '0')).join('');
        newUid = uidHex;
        
        // Convert ATQA bytes to hex string
        if (data.atqa && Array.isArray(data.atqa)) {
          newAtqa = data.atqa.map((b: number) => b.toString(16).toUpperCase().padStart(2, '0')).join('');
        }
        
        // SAK is a single byte
        if (data.sak !== undefined) {
          newSak = data.sak.toString(16).toUpperCase().padStart(2, '0');
        }
        
        scanStatus = `Fob detected! UID: ${uidHex}${newAtqa ? ` ATQA: ${newAtqa}` : ''}${newSak ? ` SAK: ${newSak}` : ''}`;
        
        // Auto-stop scan mode after successful detection
        scanMode = false;
        scanStatus = '';
      }
    }
  }

  // Subscribe to WebSocket events when scan mode is enabled
  $effect(() => {
    if (scanMode) {
      scanStatus = 'Scanning... Tap a fob near the reader.';
      return ws.on(handleNfcTagEvent);
    }
  });

  const loadFobs = async (): Promise<void> => {
    loading = true;
    try {
      const result = await getNfcFobs();
      if (result.success && result.data) {
        nfcFobConfig = result.data;
      } else if (isApiError(result)) {
        error = result.error;
      }
    } catch (e) {
      const message = e instanceof Error ? e.message : String(e);
      error = message;
    } finally {
      loading = false;
    }
  };

  const saveConfig = async (e: any): Promise<void> => {
    try {
      e.preventDefault();
      e.stopPropagation();
      const result = await saveNfcFobs(nfcFobConfig);
      if (result.success) {
        await loadFobs();
      } else if (isApiError(result)) {
        error = result.error;
      }
    } catch (e) {
      const message = e instanceof Error ? e.message : String(e);
      error = message;
      alert(`Error saving config: ${message}`);
    }
  };

  const addFob = async (e: any): Promise<void> => {
    e.preventDefault();
    e.stopPropagation();
    addError = '';
    if (!newUid.trim()) {
      addError = 'UID is required';
      return;
    }
    // Normalize UID to uppercase
    const uid = newUid.trim().toUpperCase();
    const atqa = newAtqa.trim().toUpperCase();
    const sak = newSak.trim().toUpperCase();
    const result = await addNfcFob(uid, newLabel.trim(), atqa || undefined, sak || undefined);
    if (result.success) {
      newUid = '';
      newLabel = '';
      newAtqa = '';
      newSak = '';
      await loadFobs();
    } else if (isApiError(result)) {
      addError = result.error;
    }
  };

  const toggleScanMode = () => {
    scanMode = !scanMode;
    scanStatus = scanMode ? 'Scanning... Tap a fob near the reader.' : '';
    if (!scanMode) {
      scanStatus = '';
    }
    if (scanMode) {
      newUid = '';
      newAtqa = '';
      newSak = '';
      newLabel = '';
    }
  };

  const removeFob = async (uid: string): Promise<void> => {
    const result = await deleteNfcFob(uid);
    if (result.success) {
      await loadFobs();
    } else if (isApiError(result)) {
      error = result.error;
    }
  };

  $effect(() => {
    loadFobs();
  });
</script>

<div class="w-full py-6">
  <!-- Header -->
  <div class="mb-6">
    <h1 class="text-2xl font-bold text-base-content flex items-center gap-2">
      <svg
        xmlns="http://www.w3.org/2000/svg"
        fill="none"
        viewBox="0 0 24 24"
        stroke-width="1.5"
        stroke="currentColor"
        class="size-6"
      >
        <path
          stroke-linecap="round"
          stroke-linejoin="round"
          d="M9.348 14.651a3.75 3.75 0 0 1 0-5.303m5.304 0a3.75 3.75 0 0 1 0 5.303m-7.425 2.122a6.75 6.75 0 0 1 0-9.546m9.546 0a6.75 6.75 0 0 1 0 9.546M5.106 18.894c-3.808-3.808-3.808-9.98 0-13.788m13.788 0c3.808 3.808 3.808 9.98 0 13.788M12 12h.008v.008H12V12Z"
        />
      </svg>
      NFC Fobs
    </h1>
    <p class="text-sm text-base-content/60">Manage MIFARE NFC fobs for authentication.</p>
  </div>

  <!-- Loading State -->
  {#if loading}
    <div class="flex justify-center items-center py-12">
      <span class="loading loading-spinner loading-primary"></span>
      <span class="ml-2 text-base-content/60">Loading NFC fobs...</span>
    </div>
  {:else}
    <!-- Tabs -->
    <div class="flex bg-base-300 p-1 rounded-xl gap-1 mb-4 max-w-md">
      <button
        type="button"
        class="flex-1 py-2 rounded-lg transition-colors text-sm {activeTab === 'config' ? 'bg-base-100 text-primary font-medium shadow-sm' : 'text-base-content/60 hover:bg-base-200'}"
        onclick={() => activeTab = 'config'}
      >
        Configuration
      </button>
      <button
        type="button"
        class="flex-1 py-2 rounded-lg transition-colors text-sm {activeTab === 'fobs' ? 'bg-base-100 text-primary font-medium shadow-sm' : 'text-base-content/60 hover:bg-base-200'}"
        onclick={() => activeTab = 'fobs'}
      >
        Fob List ({nfcFobConfig.fobs.length})
      </button>
    </div>

    <!-- Config Tab -->
    {#if activeTab === 'config'}
      <form onsubmit={saveConfig} class="max-w-lg space-y-4">
        <div class="card bg-base-200">
          <div class="card-body p-4">
            <div class="flex items-center justify-between">
              <div>
                <h3 class="font-semibold text-lg">NFC Fob Authentication</h3>
                <p class="text-sm text-base-content/60">Enable MIFARE NFC fob authentication alongside HomeKey</p>
              </div>
              <label class="flex items-center gap-3 cursor-pointer">
                <input
                  type="checkbox"
                  class="toggle toggle-primary"
                  bind:checked={nfcFobConfig.enabled}
                />
                <span class="text-sm text-base-content/60">Enabled</span>
              </label>
            </div>
          </div>
        </div>

        <div class="flex gap-2">
          <button type="submit" class="btn btn-primary flex-1">
            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-5">
              <path stroke-linecap="round" stroke-linejoin="round" d="M6 12 3.269 3.125A59.769 59.769 0 0 1 21.485 12 59.768 59.768 0 0 1 3.27 20.875L5.999 12Zm0 0h7.5" />
            </svg>
            Save & Apply
          </button>
        </div>

        {#if error}
          <div class="alert alert-error text-sm">
            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-5">
              <path stroke-linecap="round" stroke-linejoin="round" d="m9.75 9.75 4.5 4.5m0-4.5-4.5 4.5M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0Z" />
            </svg>
            <span>{error}</span>
          </div>
        {/if}
      </form>
    {/if}

    <!-- Fob List Tab -->
    {#if activeTab === 'fobs'}
      <div class="space-y-4">
        <!-- Add Fob Form -->
        <div class="card bg-base-200">
          <div class="card-body p-4">
            <div class="flex items-center justify-between mb-2">
              <h3 class="font-semibold flex items-center gap-2">
                <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-5 text-primary">
                  <path stroke-linecap="round" stroke-linejoin="round" d="M12 4.5v15m7.5-7.5h-15" />
                </svg>
                Add New Fob
              </h3>
              <button
                type="button"
                class="btn btn-sm {scanMode ? 'btn-warning' : 'btn-ghost'}"
                onclick={toggleScanMode}
              >
                {#if scanMode}
                  <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-4">
                    <path stroke-linecap="round" stroke-linejoin="round" d="M6 18 18 6M6 6l12 12" />
                  </svg>
                  Stop Scan
                {:else}
                  <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-4">
                    <path stroke-linecap="round" stroke-linejoin="round" d="M9.348 14.651a3.75 3.75 0 0 1 0-5.303m5.304 0a3.75 3.75 0 0 1 0 5.303m-7.425 2.122a6.75 6.75 0 0 1 0-9.546m9.546 0a6.75 6.75 0 0 1 0 9.546M5.106 18.894c-3.808-3.808-3.808-9.98 0-13.788m13.788 0c3.808 3.808 3.808 9.98 0 13.788M12 12h.008v.008H12V12Z" />
                  </svg>
                  Scan Fob
                {/if}
              </button>
            </div>
            {#if scanMode}
              <div class="bg-warning/10 border border-warning/30 rounded-lg p-3 mb-3">
                <p class="text-sm text-warning font-medium flex items-center gap-2">
                  <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-4">
                    <path stroke-linecap="round" stroke-linejoin="round" d="M9.348 14.651a3.75 3.75 0 0 1 0-5.303m5.304 0a3.75 3.75 0 0 1 0 5.303m-7.425 2.122a6.75 6.75 0 0 1 0-9.546m9.546 0a6.75 6.75 0 0 1 0 9.546M5.106 18.894c-3.808-3.808-3.808-9.98 0-13.788m13.788 0c3.808 3.808 3.808 9.98 0 13.788M12 12h.008v.008H12V12Z" />
                  </svg>
                  {scanStatus || 'Tap a fob near the reader...'}
                </p>
              </div>
            {/if}
            <form onsubmit={addFob} class="space-y-3">
              <div class="grid grid-cols-1 md:grid-cols-2 gap-3">
                <div class="form-control">
                  <label class="label">
                    <span class="label-text text-sm">Fob UID (hex)</span>
                  </label>
                  <input
                    type="text"
                    bind:value={newUid}
                    placeholder="A1B2C3D4"
                    class="input input-bordered w-full font-mono"
                    maxlength="32"
                  />
                  <label class="label">
                    <span class="label-text-alt text-xs text-base-content/50">{scanMode ? 'Auto-filled by scan' : 'Enter UID manually or use Scan Fob'}</span>
                  </label>
                </div>
                <div class="form-control">
                  <label class="label">
                    <span class="label-text text-sm">Label (optional)</span>
                  </label>
                  <input
                    type="text"
                    bind:value={newLabel}
                    placeholder="My Key Fob"
                    class="input input-bordered w-full"
                    maxlength="50"
                  />
                </div>
              </div>
              <div class="grid grid-cols-2 gap-3">
                <div class="form-control">
                  <label class="label">
                    <span class="label-text text-sm">ATQA (hex, optional)</span>
                  </label>
                  <input
                    type="text"
                    bind:value={newAtqa}
                    placeholder="0004"
                    class="input input-bordered w-full font-mono"
                    maxlength="4"
                  />
                  <label class="label">
                    <span class="label-text-alt text-xs text-base-content/50">ISO14443A ATQA</span>
                  </label>
                </div>
                <div class="form-control">
                  <label class="label">
                    <span class="label-text text-sm">SAK (hex, optional)</span>
                  </label>
                  <input
                    type="text"
                    bind:value={newSak}
                    placeholder="08"
                    class="input input-bordered w-full font-mono"
                    maxlength="2"
                  />
                  <label class="label">
                    <span class="label-text-alt text-xs text-base-content/50">ISO14443A SAK</span>
                  </label>
                </div>
              </div>
              <p class="text-xs text-base-content/50 italic">
                ATQA and SAK provide anti-cloning protection. When both are set, only a fob matching UID + ATQA + SAK will authenticate.
              </p>
              {#if addError}
                <div class="text-error text-sm">{addError}</div>
              {/if}
              <button type="submit" class="btn btn-primary btn-sm">
                <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-4">
                  <path stroke-linecap="round" stroke-linejoin="round" d="M12 9v6m3-3H9m12 0a9 9 0 1 1-18 0 9 9 0 0 1 18 0Z" />
                </svg>
                Add Fob
              </button>
            </form>
          </div>
        </div>

        <!-- Fob List -->
        <div class="card bg-base-200">
          <div class="card-body p-4">
            <h3 class="font-semibold flex items-center gap-2">
              <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-5">
                <path stroke-linecap="round" stroke-linejoin="round" d="M16.023 9.348h4.992v-.001M2.985 19.644v-4.992m0 0h4.992m-4.993 0 3.181 3.183a8.25 8.25 0 0 0 13.803-3.7M4.031 9.865a8.25 8.25 0 0 1 13.803-3.7l3.181 3.182m0-4.991v4.99" />
              </svg>
              Registered Fobs
            </h3>

            {#if nfcFobConfig.fobs.length === 0}
              <div class="text-center py-8 text-base-content/40">
                <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-12 mx-auto mb-2">
                  <path stroke-linecap="round" stroke-linejoin="round" d="M16.023 9.348h4.992v-.001M2.985 19.644v-4.992m0 0h4.992m-4.993 0 3.181 3.183a8.25 8.25 0 0 0 13.803-3.7M4.031 9.865a8.25 8.25 0 0 1 13.803-3.7l3.181 3.182m0-4.991v4.99" />
                </svg>
                <p class="text-sm">No fobs registered yet.</p>
                <p class="text-xs">Add a fob using the form above.</p>
              </div>
            {:else}
              <div class="overflow-x-auto">
                <table class="table table-zebra w-full">
                  <thead>
                    <tr>
                      <th>#</th>
                      <th>UID</th>
                      <th>ATQA</th>
                      <th>SAK</th>
                      <th>Label</th>
                      <th class="text-right">Actions</th>
                    </tr>
                  </thead>
                  <tbody>
                    {#each nfcFobConfig.fobs as fob, index}
                      <tr>
                        <td>{index + 1}</td>
                        <td class="font-mono text-sm">{fob.uid}</td>
                        <td class="font-mono text-sm">{#if fob.atqa}{fob.atqa}{:else}<span class="text-base-content/40 italic">—</span>{/if}</td>
                        <td class="font-mono text-sm">{#if fob.sak}{fob.sak}{:else}<span class="text-base-content/40 italic">—</span>{/if}</td>
                        <td>{#if fob.label}{fob.label}{:else}<span class="text-base-content/40 italic">No label</span>{/if}</td>
                        <td class="text-right">
                          <button
                            class="btn btn-ghost btn-xs btn-error"
                            onclick={() => removeFob(fob.uid)}
                          >
                            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-4">
                              <path stroke-linecap="round" stroke-linejoin="round" d="m14.74 9-3.262 3.263m4.524-4.524-3.263 3.262M3 10.5 4.5 9m0 0L6 10.5m-3 0h6m6 0 1.5 1.5m0 0L18 10.5m0 0-1.5-1.5m1.5 1.5h-6" />
                            </svg>
                            Delete
                          </button>
                        </td>
                      </tr>
                    {/each}
                  </tbody>
                </table>
              </div>
            {/if}
          </div>
        </div>
      </div>
    {/if}
  {/if}
</div>
