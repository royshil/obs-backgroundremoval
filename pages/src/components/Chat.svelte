<script lang="ts">
  import { writable } from 'svelte/store';
  import type { UIMessage } from '@ai-sdk/svelte';

  // Helper type for a simple text part for clarity
  type TextPart = { type: 'text'; text: string };

  // Helper function to create a new message
  const createMessage = (role: 'user' | 'assistant', text: string): UIMessage => ({
    id: Date.now().toString() + '-' + role,
    role,
    // Cast to UIMessage is usually fine if structure matches
    parts: [{ type: 'text', text }] as UIMessage['parts'],
    metadata: {
      createdAt: new Date(),
    },
  });

  // Initialize messages with an initial assistant greeting
  const initialMessage = createMessage(
    'assistant',
    'Hello! This is a mock simple chat UI awaiting transition to WebLLM.'
  );

  const messages = writable<UIMessage[]>([initialMessage]);
  let input = '';
  let isLoading = false;
  let messagesEnd: HTMLDivElement;
  
  // Reactive statement to scroll to the bottom when messages change
  // Svelte's idiomatic way to handle side effects from reactive state changes.
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

  /** Handles the form submission. */
  const handleSubmit = (event: SubmitEvent) => {
    event.preventDefault();
    if (!input.trim() || isLoading) return;

    const userMessageContent = input;
    const userMessage = createMessage('user', userMessageContent);

    // Add user message and clear input
    messages.update(msgs => [...msgs, userMessage]);
    input = '';
    
    handleMockResponse(userMessageContent);
  };
  
  /** Generates a mock assistant response after a delay. */
  async function handleMockResponse(userMessageContent: string) {
    isLoading = true;
    // Simulate network delay
    await new Promise(resolve => setTimeout(resolve, 800 + Math.random() * 500)); 

    let mockContent = '...Received mock response. Looking forward to the WebLLM implementation!';
    
    // Basic keyword-based response logic
    const lowerInput = userMessageContent.toLowerCase();
    if (lowerInput.includes('astro')) {
        mockContent = 'Confirmed Astro and Svelte integration. Please proceed with WebLLM embedding.';
    } else if (lowerInput.includes('ui')) {
        mockContent = 'The UI remains simple. It can be used as is after migrating to WebLLM.';
    }

    const mockMessage = createMessage('assistant', mockContent);

    messages.update(msgs => [...msgs, mockMessage]);
    isLoading = false;
  }
</script>

---

<div class="chat-container">
  <div class="message-list">
    {#each $messages as message (message.id)}
      <div 
        class="message-bubble" 
        class:user={message.role === 'user'} 
        class:assistant={message.role === 'assistant'}
      >
        <span class="role">{message.role === 'user' ? 'You' : 'AI Mock'}:</span>
        <p>{(message.parts[0] as TextPart)?.text || '(No text content)'}</p>
      </div>
    {/each}
    
    {#if isLoading}
        <div class="loading-indicator message-bubble assistant">
            <span class="role">AI Mock:</span>
            <p>Thinking...</p>
        </div>
    {/if}
    
    <div bind:this={messagesEnd} style="height: 0;"></div>
  </div>

  <form onsubmit={handleSubmit}>
    <input
      bind:value={input}
      type="text"
      placeholder={isLoading ? 'Waiting for response...' : 'Type a message...'}
      disabled={isLoading}
      required
    />
    <button type="submit" disabled={isLoading || !input.trim()}>
      {isLoading ? 'Sending' : 'Send'}
    </button>
  </form>
</div>

<style>
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
    transition: background-color 0.2s;
  }

  button:disabled {
    background-color: #999;
    cursor: not-allowed;
  }
</style>
