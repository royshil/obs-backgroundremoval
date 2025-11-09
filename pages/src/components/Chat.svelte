<script lang="ts">
  import { writable } from 'svelte/store';
  import { llmStore } from '../lib/llm.ts'; // 適切なパスに修正してください
  import type { MLCEngineInterface, ChatCompletionMessageParam } from '@mlc-ai/web-llm';

  import FaqContent from '../../../FAQ.md?raw';

  // LLMの型とは独立した、UI用のメッセージ構造を定義
  type SimpleMessage = {
    id: string;
    role: 'user' | 'assistant';
    // partsの構造は元のUIMessageから継承
    parts: { type: 'text'; text: string }[];
    metadata?: {
      createdAt: Date;
    };
  };

  // Helper type for a simple text part for clarity
  type TextPart = { type: 'text'; text: string };

  // Helper function to create a new message
  const createMessage = (role: 'user' | 'assistant', text: string): SimpleMessage => ({
    id: Date.now().toString() + '-' + role,
    role,
    // Castを SimpleMessage['parts'] に修正
    parts: [{ type: 'text', text }] as SimpleMessage['parts'],
    metadata: {
      createdAt: new Date(),
    },
  });

  // Initialize messages with an initial assistant greeting
  const initialMessage = createMessage(
    'assistant',
    'Hello! This is a mock simple chat UI awaiting transition to WebLLM.'
  );

  // writableの型を SimpleMessage[] に変更
  const messages = writable<SimpleMessage[]>([initialMessage]);
  let input = '';
  let isLoading = false; // 会話中のローディング
  let hasFaqBeenPreloaded = false;
  let messagesEnd: HTMLDivElement;
  
  // LLMストアの状態を監視
  const llmState = llmStore;
  
  // LLMの状態を判定
  $: isLLMReady = $llmState.status === 'ready';
  $: isLLMLoading = $llmState.status === 'loading';
  $: isLLMPending = $llmState.status === 'pending';

  $: if (isLLMReady && !hasFaqBeenPreloaded) {
    hasFaqBeenPreloaded = true; // フラグを設定し、二度目の実行を防ぐ
    
    // UIをブロックしないように非同期で実行
    // ユーザーには見えないバックグラウンド処理
    handleFaqPreload(); 
  }

  // Reactive statement to scroll to the bottom when messages change
  $: $messages, scrollToBottom();

  /** Scrolls the chat view to the bottom. */
  function scrollToBottom() {
    if (messagesEnd) {
      // Use setTimeout to ensure the DOM has updated before scrolling
      setTimeout(() => {
        messagesEnd.scrollIntoView({ behavior: 'smooth' });
      }, 0);
    }
  }

  /** ユーザーがダウンロードに同意し、LLMの初期化を開始するハンドラ */
  function handleAgreeAndStart() {
    if (isLLMPending) {
      llmStore.startLLMInitialization();
    }
  }

  /** Handles the form submission. */
  const handleSubmit = (event: SubmitEvent) => {
    event.preventDefault();
    
    // LLMがReadyでない場合は、メッセージ送信を許可しない
    if (!input.trim() || isLoading || !isLLMReady) return; 

    const userMessageContent = input;
    const userMessage = createMessage('user', userMessageContent);

    // Add user message and clear input
    messages.update(msgs => [...msgs, userMessage]);
    input = '';
    
    handleWebLLMChat(userMessageContent);
  };
  

      // FAQコンテンツを含むシステムプロンプトの構築
      const systemPrompt = `
You are a helpful AI assistant. 
Your knowledge base is provided below. Always refer to this knowledge first when answering questions related to it.

--- KNOWLEDGE BASE (FAQ) ---
${FaqContent}
---------------------------
`;

  /** WebLLMを使用してアシスタントの応答を生成します。 */
  async function handleWebLLMChat(userMessageContent: string) {
    if ($llmState.status !== 'ready') {
        console.error('LLM is not ready.');
        return;
    }

    // MLCEngineInterfaceを取得
    const engine: MLCEngineInterface = $llmState.chat;
    let assistantResponse = '';
    isLoading = true;

    try {
        // メッセージ履歴の構築
        const messagesToSend = [
            { role: "system", content: systemPrompt }, 
            { role: "user", content: userMessageContent }
        ] satisfies ChatCompletionMessageParam[]; // 型安全性を保つために satisfies を使用

        // ストリーミング中に一時的にメッセージをプレースホルダーとして追加
        messages.update(msgs => [...msgs, createMessage('assistant', '')]);
        
        // engine.chat.completions インターフェースを使用
        const responseStream = await engine.chat.completions.create({
            messages: messagesToSend,
            stream: true, // ストリーミングを有効化
        });

        // ストリームの処理
        for await (const chunk of responseStream) {
            const content = chunk.choices[0]?.delta.content; // deltaを使用
            if (content) {
                assistantResponse += content;
                
                // 最後のメッセージを更新（ストリーミング）
                messages.update(msgs => {
                    const lastMessage = msgs[msgs.length - 1];
                    if (lastMessage && lastMessage.role === 'assistant') {
                        // ストリーミング中にDOMを更新
                        return [...msgs.slice(0, -1), createMessage('assistant', assistantResponse)];
                    }
                    return msgs; 
                });
            }
        }
        
    } catch (error) {
        console.error('LLM conversation failed:', error);
        assistantResponse = 'Error: LLM failed to generate a response.';
        // エラーが発生した場合、最後のメッセージをエラーメッセージで上書き
        messages.update(msgs => {
             return [...msgs.slice(0, -1), createMessage('assistant', assistantResponse)];
        });
        
    } finally {
        isLoading = false;
    }
  }

  /** LLMがReadyになった直後にFAQをシステムプロンプトとしてプリロードします。 */
  async function handleFaqPreload() {
      console.log("LLM ready. Preloading FAQ knowledge base...");
    
      // ダミーのユーザープロンプト (プリロードが目的のため、短いメッセージで十分)
      const dummyPrompt = "Start a new session. Do not respond with a greeting, just acknowledge the system prompt and confirm readiness.";

      try {
          if ($llmState.status !== 'ready') {
              console.error('LLM is not ready for FAQ preload.');
              return;
          }
          const engine: MLCEngineInterface = $llmState.chat;
          
          // メッセージ履歴の構築
          const messagesToSend = [
              { role: "system", content: systemPrompt }, 
              { role: "user", content: dummyPrompt }
          ] satisfies ChatCompletionMessageParam[]; 

          // ストリーミングなし（stream: false）で一度だけ推論を実行
          // これにより、FAQの内容がモデルの内部キャッシュ（コンテキスト）にロードされます。
          await engine.chat.completions.create({
              messages: messagesToSend,
              stream: false, // 応答は不要
          });

          console.log("FAQ preload complete. LLM is now ready for efficient chat.");

      } catch (error) {
          console.error('FAQ preload failed:', error);
          // エラーが発生した場合、ユーザーチャットでFAQが渡され続けるため、致命的ではない
      }
  }
</script>

---

<div class="chat-container">
  <div class="message-list">
    
    {#if isLLMPending}
        <div class="initial-warning message-bubble assistant">
            <span class="role">System Warning:</span>
            <p>
                このチャット機能を利用するには、**Llama-3.1-8B-Instruct-q4f16_1-MLC** モデル（数GBの大きなファイル）をダウンロードする必要があります。
                初回起動時にのみ必要ですが、完了まで数分かかる場合があります（インターネット接続速度に依存）。
            </p>
            <button class="agree-button" onclick={handleAgreeAndStart}>
                同意してモデルのダウンロードを開始する
            </button>
        </div>
    {/if}

    {#if !isLLMLoading && !isLLMPending}
        {#each $messages as message (message.id)}
          <div 
            class="message-bubble" 
            class:user={message.role === 'user'} 
            class:assistant={message.role === 'assistant'}
          >
            <span class="role">{message.role === 'user' ?
'You' : 'AI Mock'}:</span>
            <p>{(message.parts[0] as TextPart)?.text ||
'(No text content)'}</p>
          </div>
        {/each}
    {/if}
    
    <div bind:this={messagesEnd} style="height: 0;"></div>
  </div>

  {#if isLoading || isLLMLoading}
    <div class="loading-indicator">
        <div class="spinner-container">
            <div><div class="spinner"></div></div>
            <p class="loading-text">
                {isLLMLoading 
                    ? ($llmState.status === 'loading' ? `ダウンロード中: ${$llmState.message}` : 'Initializing...') 
                    : '思考中...'}
            </p>
        </div>
    </div>
  {/if}

  <form onsubmit={handleSubmit}>
    <input
      bind:value={input}
      type="text"
      placeholder={!isLLMReady ? 
        ($llmState.status === 'error' ? 'エラーが発生しました' : isLLMPending ? '開始ボタンを押してください' : 'モデルを読み込み中...')
        : (isLoading ? '応答待ち...' : 'メッセージを入力...')
      }
      disabled={isLoading || !isLLMReady || isLLMLoading || isLLMPending}
      required
    />
    <button type="submit" disabled={isLoading || !input.trim() || !isLLMReady || isLLMLoading || isLLMPending}>
      {isLoading ? '送信中' : (!isLLMReady ? '待機中' : '送信')}
    </button>
  </form>
</div>

<style>
  .chat-container {
    /* position: relative; は不要 */
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
  
  /* 警告と同意ボタンのためのスタイル */
  .initial-warning {
    background-color: #fff3cd; /* 警告色 */
    color: #856404;
    border: 1px solid #ffeeba;
    padding: 15px;
    border-radius: 8px;
    max-width: 100%;
    margin-bottom: 20px;
  }

  .initial-warning p {
    margin-bottom: 10px;
  }
  
  .agree-button {
    background-color: #28a745; /* Green */
    color: white;
    padding: 10px 15px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    margin-top: 10px;
    display: block;
    width: 100%;
    transition: background-color 0.2s;
  }

  .agree-button:hover {
    background-color: #218838;
  }

  /* ローディングインジケータ（メッセージバブルの外） */
  .loading-indicator {
    /* 修正: position: absolute; を削除し、フロー内に配置 */
    /* bottom, left, transform, z-index も全て削除 */
    
    margin: 5px 15px 10px 15px; /* マージンで配置を調整 */
    padding: 8px 12px; /* message-bubbleと同じパディング */
    align-self: flex-start; /* アシスタントのように左寄せ */
    background-color: #e5e5ea; /* アシスタントバブルと同じ背景色 */
    color: #000;
    border-radius: 18px; /* message-bubbleと同じ角丸 */
    box-shadow: 0 1px 1px rgba(0, 0, 0, 0.05);
    max-width: 50%; /* 幅を制限 */
    
    /* 内部のフレックスレイアウト */
    display: inline-flex; /* contentの幅に合わせる */
    align-items: center;
    gap: 10px;
  }

  .spinner-container {
    display: flex;
    align-items: center;
    gap: 8px;
  }

  .spinner {
    border: 4px solid rgba(0, 0, 0, 0.1);
    border-left-color: #007bff; /* スピナーの色 */
    border-radius: 50%;
    width: 20px;
    height: 20px;
    animation: spin 1s linear infinite;
  }

  @keyframes spin {
    0% { transform: rotate(0deg); }
    100% { transform: rotate(360deg); }
  }

  .loading-text {
    margin: 0; /* pタグのデフォルトマージンをリセット */
    font-size: 0.9em;
    color: #333;
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
    transition: background-color 0.2s;
  }

  button:disabled {
    background-color: #999;
    cursor: not-allowed;
  }
</style>
