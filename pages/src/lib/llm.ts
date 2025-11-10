// src/stores/llm.ts

import { writable, type Writable } from "svelte/store";
import {
  CreateMLCEngine,
  type MLCEngineInterface,
  type InitProgressCallback,
  type InitProgressReport, // Add InitProgressReport for clarity on progress object structure
} from "@mlc-ai/web-llm";

// LLM execution state type
export type LLMState =
  | { status: "uninitialized" }
  | { status: "pending" } // Waiting for user consent to start download
  | { status: "loading"; progress: number; message: string }
  | { status: "ready"; chat: MLCEngineInterface } // Holds the initialized MLCEngineInterface
  | { status: "error"; error: string };

/**
 * Creates a Svelte store to manage the lifecycle and state of the MLCEngine.
 * * @param modelId The ID of the model to load (e.g., 'Llama-3.1-8B-Instruct-q4f16_1-MLC').
 * @returns A store interface with subscribe and initialization methods.
 */
const createLLMStore = (modelId: string) => {
  // Initial state is 'pending', waiting for user consent
  const { subscribe, set, update } = writable<LLMState>({ status: "pending" });

  // Note: For MLCEngine, the 'progress' property in InitProgressReport is typically used
  // to directly track the overall load progress from 0 to 1.

  /**
   * Progress callback function to monitor MLCEngine loading and update the Svelte store.
   */
  const initProgressCallback: InitProgressCallback = (
    progress: InitProgressReport,
  ) => {
    // Update the state using the progress report data
    update((state) => ({
      ...state,
      status: "loading",
      // Use the progress property directly if it's available and represents 0-1 ratio
      progress: progress.progress,
      // Current download/load message text
      message: progress.text,
    }));
  };

  /**
   * Starts MLCEngine initialization and model downloading after user consent.
   * This handles the asynchronous loading process.
   */
  const startLLMInitialization = async () => {
    // Set loading state initially
    set({ status: "loading", progress: 0, message: "Creating MLCEngine..." });

    try {
      // CreateMLCEngine creates and loads the engine with the specified model
      const engine: MLCEngineInterface = await CreateMLCEngine(modelId, {
        initProgressCallback,
      });

      // Set to 'ready' state upon successful initialization
      set({ status: "ready", chat: engine });
    } catch (error) {
      console.error("WebLLM initialization failed:", error);
      set({
        status: "error",
        error: error instanceof Error ? error.message : "Unknown error",
      });
    }
  };

  return {
    subscribe,
    startLLMInitialization, // Public method to start initialization
  };
};

// Initialize the store by passing the default model ID
export const llmStore = createLLMStore("Hermes-3-Llama-3.2-3B-q4f16_1-MLC");
