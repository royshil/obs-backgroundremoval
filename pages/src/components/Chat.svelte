<script lang="ts">
  import { onMount } from 'svelte';
  import { writable } from 'svelte/store';
  
  // Vercel AI SDKから型をインポート
  import type { UIMessage } from '@ai-sdk/svelte'; 

  // カスタム型を含まない基本的な UIMessage を定義
  type BaseUIMessage = UIMessage;

  // TextUIPartの構造を定義 (または、シンプルにするために型キャストで対応)
  // 💡 TextUIPart の構造を直接定義する
  type TextPart = { type: 'text'; text: string; };

  const messages = writable<BaseUIMessage[]>([]);
  let input = '';
  let isLoading = false;
  let messagesEnd: HTMLDivElement;
  
  // メッセージが更新されたら一番下までスクロール
  messages.subscribe(() => {
    if (messagesEnd) {
      setTimeout(() => {
        messagesEnd.scrollIntoView({ behavior: 'smooth' });
      }, 0); 
    }
  });

  onMount(() => {
    // 初期メッセージ: parts 構造を使用
    messages.set([
      { 
        id: '1', 
        role: 'assistant', 
        parts: [
          // TextUIPart の type: 'text' と text プロパティを使用
          { type: 'text', text: 'こんにちは！これはWebLLMへの移行を待つシンプルなチャットUIのモックです。' } satisfies TextPart
        ],
        // createdAt は metadata に含める（ただし表示ロジックは簡略化）
        metadata: {
          createdAt: new Date()
        }
      },
    ]);
  });
  
  /**
   * メッセージから表示用のテキストを取得するヘルパー関数
   * 最初の 'text' タイプの part のみを取得します。
   */
  function getTextForDisplay(message: BaseUIMessage): string {
    const textPart = message.parts.find(part => part.type === 'text') as TextPart | undefined;
    return textPart?.text || '（テキストコンテンツがありません）';
  }

  const handleSubmit = (event: SubmitEvent) => {
    console.log(event)
    event.preventDefault();
    if (!input.trim() || isLoading) return;

    const userMessageContent = input;
    const newMessage: BaseUIMessage = { 
        id: Date.now().toString() + '-user', 
        role: 'user', 
        parts: [
          { type: 'text', text: userMessageContent } satisfies TextPart
        ],
        metadata: {
          createdAt: new Date()
        }
    };
    messages.update(msgs => [...msgs, newMessage]);
    input = '';
    handleMockResponse(userMessageContent);
  };
  
  async function handleMockResponse(userMessageContent: string) {
    isLoading = true;
    await new Promise(resolve => setTimeout(resolve, 800 + Math.random() * 500)); 

    let mockContent = '...モック応答を受信しました。WebLLMの実装を楽しみにしています！';
    
    if (userMessageContent.toLowerCase().includes('astro')) {
        mockContent = 'AstroとSvelteの連携を確認しました。WebLLMの組み込みに進んでください。';
    } else if (userMessageContent.toLowerCase().includes('ui')) {
        mockContent = 'UIはシンプルさを保ちます。WebLLMへの移行後もそのまま使えます。';
    }

    const mockMessage: BaseUIMessage = { 
        id: Date.now().toString() + '-assistant', 
        role: 'assistant', 
        parts: [
          { type: 'text', text: mockContent } satisfies TextPart
        ],
        metadata: {
          createdAt: new Date()
        }
    };

    messages.update(msgs => [...msgs, mockMessage]);
    isLoading = false;
  }
</script>

<div class="chat-container">
  <div class="message-list">
    {#each $messages as message (message.id)}
      <div class="message-bubble" class:user={message.role === 'user'} class:assistant={message.role === 'assistant'}>
        <span class="role">{message.role === 'user' ? 'あなた' : 'AIモック'}:</span>
        <p>{getTextForDisplay(message)}</p>
      </div>
    {/each}
    
    {#if isLoading}
        <div class="loading-indicator message-bubble assistant">
            <span class="role">AIモック:</span>
            <p>思考中...</p>
        </div>
    {/if}
    
    <div bind:this={messagesEnd} style="height: 0;"></div>
  </div>

  <form onsubmit={handleSubmit}>
    <input
      bind:value={input}
      type="text"
      placeholder={isLoading ? '応答待ち...' : 'メッセージを入力...'}
      disabled={isLoading}
    />
    <button type="submit" disabled={isLoading}>
      {isLoading ? '送信中' : '送信'}
    </button>
  </form>
</div>

<style>
  /* CSSは以前のシンプルなバージョン（ローディングアニメーションのみ追加） */
  .chat-container {
    display: flex;
    flex-direction: column;
    height: 80vh; 
    max-width: 600px;
    margin: 0 auto;
    border: 1px solid #ccc;
    border-radius: 8px;
    overflow: hidden;
  }
  
  .message-list {
    flex-grow: 1;
    padding: 15px;
    overflow-y: auto;
    background-color: #f9f9f9;
  }
  
  .message-bubble {
    margin-bottom: 10px;
    padding: 8px 12px;
    border-radius: 18px;
    max-width: 80%;
    word-wrap: break-word;
    box-shadow: 0 1px 1px rgba(0,0,0,0.05);
  }
  
  .user {
    align-self: flex-end;
    background-color: #007aff;
    color: white;
    margin-left: auto;
  }
  
  .assistant {
    align-self: flex-start;
    background-color: #e5e5ea;
    color: #000;
  }
  
  .role {
    font-size: 0.8em;
    font-weight: bold;
    display: block;
    margin-bottom: 3px;
    opacity: 0.7;
  }

  .loading-indicator {
    opacity: 0.8;
    animation: pulse 1.5s infinite;
  }

  @keyframes pulse {
    0% { opacity: 0.8; }
    50% { opacity: 0.5; }
    100% { opacity: 0.8; }
  }

  form {
    display: flex;
    padding: 10px;
    border-top: 1px solid #ccc;
    background-color: white;
  }
  
  input {
    flex-grow: 1;
    padding: 10px;
    border: 1px solid #ddd;
    border-radius: 4px;
    margin-right: 10px;
  }
  
  button {
    padding: 10px 15px;
    background-color: #007aff;
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
  }

  button:disabled {
    background-color: #999;
    cursor: not-allowed;
  }
</style>