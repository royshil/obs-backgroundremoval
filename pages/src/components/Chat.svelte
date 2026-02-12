<script lang="ts">
  import { writable } from "svelte/store";
  import { llmStore } from "../lib/llm.ts";
  // Please correct the appropriate path
  import type {
    MLCEngineInterface,
    ChatCompletionMessageParam,
  } from "@mlc-ai/web-llm";

  import FaqContent from "../../../FAQ.md?raw";

  // Define the message structure for the UI, independent of the LLM type
  type SimpleMessage = {
    id: string;
    role: "user" | "assistant";
    // parts structure inherited from the original UIMessage
    parts: { type: "text"; text: string }[];
    metadata?: {
      createdAt: Date;
    };
  };

  // Helper type for a simple text part for clarity
  type TextPart = { type: "text"; text: string };

  // Helper function to create a new message
  const createMessage = (
    role: "user" | "assistant",
    text: string,
  ): SimpleMessage => ({
    id: Date.now().toString() + "-" + role,
    role,
    // Fix cast to SimpleMessage['parts']
    parts: [{ type: "text", text }] as SimpleMessage["parts"],
    metadata: {
      createdAt: new Date(),
    },
  });

  // Initialize messages with an initial assistant greeting
  const initialMessage = createMessage(
    "assistant",
    "Hi there! I'm Cora, your interactive support assistant. Feel free to ask me anything about the knowledge base, and I'll do my best to help you out! 🤖",
  );

  // Change writable type to SimpleMessage[]
  const messages = writable<SimpleMessage[]>([initialMessage]);
  let input = "";
  let isLoading = false; // Loading during conversation
  let messagesEnd: HTMLDivElement;

  // Monitor LLM store status
  const llmState = llmStore;

  // Determine LLM status
  $: isLLMReady = $llmState.status === "ready";
  $: isLLMLoading = $llmState.status === "loading";
  $: isLLMPending = $llmState.status === "pending";

  // Reactive statement to scroll to the bottom when messages change
  $: ($messages, scrollToBottom());

  /** Scrolls the chat view to the bottom. */
  function scrollToBottom() {
    if (messagesEnd) {
      // Use setTimeout to ensure the DOM has updated before scrolling
      setTimeout(() => {
        messagesEnd.scrollIntoView({ behavior: "smooth" });
      }, 0);
    }
  }

  /** Handler for the user agreeing to download and starting LLM initialization */
  function handleAgreeAndStart() {
    if (isLLMPending) {
      llmStore.startLLMInitialization();
    }
  }

  /** Handles the form submission. */
  const handleSubmit = (event: SubmitEvent) => {
    event.preventDefault();

    // Do not allow message submission if the LLM is not ready
    if (!input.trim() || isLoading || !isLLMReady) return;

    const userMessageContent = input;
    const userMessage = createMessage("user", userMessageContent);

    // Add user message and clear input
    messages.update((msgs) => [...msgs, userMessage]);
    input = "";

    handleWebLLMChat(userMessageContent);
  };

  // System prompt including FAQ content (Used every time)
  const systemPrompt = `
You are Cora, a friendly and helpful interactive support assistant.
Your primary goal is to provide clear, concise, and structured answers to the user's questions, prioritizing the information contained within the "KNOWLEDGE BASE" provided below.

When answering, please adhere to these guidelines:
1.  **Tone and Style:** Be friendly, encouraging, and easy to understand.
2.  **Source Priority:**
    * **Always prioritize** facts and details found in the KNOWLEDGE BASE.
    * If the KNOWLEDGE BASE **does not explicitly contain** the necessary information, you may use your general knowledge, provided you maintain a supportive tone.
3.  **Attribution:** When the answer is based on the KNOWLEDGE BASE, use a natural phrasing such as "**As far as I know from my resources**," or "**Based on the information I have**," to introduce the answer, instead of using phrases like "based ONLY on the KNOWLEDGE BASE."
4.  **Clarity and Structure:** Never quote the Q&A text verbatim, and do not use internal document formatting like 'Q:', 'A:', '###', or '**'. Use bullet points, short paragraphs, or numbered lists to present the information clearly.
5.  **Completeness:** Synthesize the most relevant facts from the knowledge base (or general knowledge if needed) to fully address the user's inquiry.

Your knowledge base is provided below. Always refer to this knowledge first when answering questions related to it.

--- KNOWLEDGE BASE (FAQ) ---
${FaqContent}
---------------------------
`;

  /** Generates the assistant's response using WebLLM. */
  async function handleWebLLMChat(userMessageContent: string) {
    if ($llmState.status !== "ready") {
      console.error("LLM is not ready.");
      return;
    }

    // Get MLCEngineInterface
    const engine: MLCEngineInterface = $llmState.chat;
    let assistantResponse = "";
    isLoading = true;

    try {
      // Construct message history: always use the long systemPrompt including the FAQ
      const messagesToSend = [
        { role: "system", content: systemPrompt },
        { role: "user", content: userMessageContent },
      ] satisfies ChatCompletionMessageParam[]; // Use satisfies for type safety

      // Temporarily add a placeholder message during streaming
      messages.update((msgs) => [...msgs, createMessage("assistant", "")]);

      // Use engine.chat.completions interface
      const responseStream = await engine.chat.completions.create({
        messages: messagesToSend,
        stream: true, // Enable streaming
      });

      // Process the stream
      for await (const chunk of responseStream) {
        const content = chunk.choices[0]?.delta.content; // Use delta
        if (content) {
          assistantResponse += content;

          // Update the last message (streaming)
          messages.update((msgs) => {
            const lastMessage = msgs[msgs.length - 1];
            if (lastMessage && lastMessage.role === "assistant") {
              // Update DOM during streaming
              return [
                ...msgs.slice(0, -1),
                createMessage("assistant", assistantResponse),
              ];
            }
            return msgs;
          });
        }
      }
    } catch (error) {
      console.error("LLM conversation failed:", error);
      assistantResponse = "Error: LLM failed to generate a response.";
      // If an error occurs, overwrite the last message with the error message
      messages.update((msgs) => {
        return [
          ...msgs.slice(0, -1),
          createMessage("assistant", assistantResponse),
        ];
      });
    } finally {
      isLoading = false;
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
          To use this chat feature, you must download the LLM model (a large
          file of several GBs). This is only required on the first launch, but
          it may take several minutes to complete (depending on your internet
          speed).
        </p>
        <button class="agree-button" onclick={handleAgreeAndStart}>
          Agree and Start Model Download
        </button>
      </div>
    {/if}

    {#if !isLLMLoading && !isLLMPending}
      {#each $messages as message (message.id)}
        <div
          class="message-bubble"
          class:user={message.role === "user"}
          class:assistant={message.role === "assistant"}
        >
          <span class="role">{message.role === "user" ? "You" : "Cora"}:</span>
          <pre style="white-space: pre-wrap; font-family: inherit;">{(
              message.parts[0] as TextPart
            )?.text || "..."}</pre>
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
            ? $llmState.status === "loading"
              ? `Downloading: ${$llmState.message}`
              : "Initializing..."
            : "Thinking..."}
        </p>
      </div>
    </div>
  {/if}

  <form onsubmit={handleSubmit}>
    <input
      bind:value={input}
      type="text"
      placeholder={!isLLMReady
        ? $llmState.status === "error"
          ? "An error occurred"
          : isLLMPending
            ? "Please press the start button"
            : "Loading model..."
        : isLoading
          ? "Waiting for response..."
          : "Enter your message..."}
      disabled={isLoading || !isLLMReady || isLLMLoading || isLLMPending}
      required
    />
    <button
      type="submit"
      disabled={isLoading ||
        !input.trim() ||
        !isLLMReady ||
        isLLMLoading ||
        isLLMPending}
    >
      {isLoading ? "Sending" : !isLLMReady ? "Waiting" : "Send"}
    </button>
  </form>
</div>

<style>
  .chat-container {
    /* position: relative; is unnecessary */
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
    box-shadow: 0 1px 1px rgba(0, 0, 0, 0.05);
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

  /* Style for warning and agree button */
  .initial-warning {
    background-color: #fff3cd; /* Warning color */
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

  /* Loading indicator (outside message bubble) */
  .loading-indicator {
    /* Fix: position: absolute; removed and placed within the flow */
    /* bottom, left, transform, z-index are also all removed */

    margin: 5px 15px 10px 15px; /* Adjust placement with margin */
    padding: 8px 12px; /* Same padding as message-bubble */
    align-self: flex-start; /* Left-aligned like assistant */
    background-color: #e5e5ea; /* Same background color as assistant bubble */
    color: #000;
    border-radius: 18px; /* Same border-radius as message-bubble */
    box-shadow: 0 1px 1px rgba(0, 0, 0, 0.05);
    max-width: 50%; /* Limit width */

    /* Internal flex layout */
    display: inline-flex; /* Fit content width */
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
    border-left-color: #007bff; /* Spinner color */
    border-radius: 50%;
    width: 20px;
    height: 20px;
    animation: spin 1s linear infinite;
  }

  @keyframes spin {
    0% {
      transform: rotate(0deg);
    }
    100% {
      transform: rotate(360deg);
    }
  }

  .loading-text {
    margin: 0; /* Reset default margin for p tag */
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
