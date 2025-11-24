//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import java.util.concurrent.Flow;
import java.util.concurrent.SubmissionPublisher;
import java.util.concurrent.atomic.AtomicBoolean;

import java.util.ArrayList;
import java.util.List;

/**
 * Llm class that extends the SubmissionPublisher
 */
public class Llm {

    private static String eosToken = "<eos>";
    private String imagePath = "";
    private boolean imageUploaded = false;
     /**
      * @brief Maximum allowed input image dimension (in pixels).
      */
    public int maxInputImageDim = 128;

    static {
        try {
            System.loadLibrary("arm-llm-jni");
        } catch (UnsatisfiedLinkError e) {
            System.err.println("Llama: Failed to load library: arm-llm-jni");
            e.printStackTrace();
        }
    }

    /**
     * Create LLM native instance from config.
     *
     * @param jsonConfig string contains configuration in json
     * @param sharedLibraryPath Path to shared library folder to load optional shared libs
     */
    public native void llmInit(String jsonConfig, String sharedLibraryPath);

    /**
     * @return Checks if the LLM impl supports Image input.
     */
    public native boolean supportsImageInput();

    /**
     * Free the LLM model (native).
     */
    private native void freeLlm();

    /**
     * @return Encoding rate in tokens/s.
     */
    public native float getEncodeRate();

    /**
     * @return Decoding rate in tokens/s.
     */
    public native float getDecodeRate();

    /**
     * Private method for resetting conversation history
     */
    public native void resetContext();

    /**
     * Reset timing information (native).
     */
    public native void resetTimings();

    /**
     * Method to encode the given text and image
     * @param text               the prompt to be encoded
     * @param pathToImage        path to the image to be encoded
     * @param isFirstMessage     boolean flag to signal if its the first message or not
     */
    private native void encode(String text, String pathToImage, boolean isFirstMessage);

    /**
     * Method to get Next Token once encoding is done.
     * This Method needs to be called in a loop while monitoring for Stop-Words.
     * @return next Token as String
     */
    public native String getNextToken();

    /**
     * Method to produce next token
     * @param operationId can be used to return an error or check for user cancel operation requests
     * @return the next Token for Encoded Prompt
     */
    public native String getNextTokenCancellable(long operationId);

    /**
     * Function to request the cancellation of a ongoing operation / functional call
     * @param operationId associated with operation / functional call
     */
    public native void cancel(long operationId);

    /**
     * @return Chat progress as percentage [0–100].
     */
    public native int getChatProgress();

    /**
     * Benchmark the model.
     *
     * @param nPrompts     Prompt length.
     * @param nEvalPrompts Number of generated tokens.
     * @param nMaxSeq      Sequence length.
     * @param nRep         Number of repetitions.
     * @return Benchmark results string.
     */
    public native String benchModel(int nPrompts, int nEvalPrompts, int nMaxSeq, int nRep);

    /**
     * @return Framework type as string.
     */
    public native String getFrameworkType();


    /**
     * Submit a query synchronously.
     *
     * @param query  User query.
     * @return void.
     */
    public void submit(String query) {

        System.out.println("Submiting query: '" + query + "'");

        if (query.length() > 0) {
            handleEncoding(query);
        }
    }

    /**
     * Submits query to LLM and returns response.
     *
     * @param query  User query.
     * @return Response string .
     */
    public String getResponse(String query) {

        if (query.length()  <= 0) {
            return "";
        }

        submit(query);

        StringBuilder response = new StringBuilder();
        while (getChatProgress() < 100) {
            String token = getNextToken();
            response.append(token);

            if (eosToken.equals(token)) {
                break;
            }
        }

        return response.toString();
    }



    private void handleEncoding(String query) {
        if (!imageUploaded) {
            encode(query, "", true);
        } else {
            encode(query, imagePath, false);
            imageUploaded = false;
        }
    }

    /**
     * Free model from memory and close publisher.
     */
    public void freeModel() {
        freeLlm();
    }

    /**
     * Set image location for the next message.
     *
     * @param imagePath Path to image file.
     */
    public void setImageLocation(String imagePath) {
        this.imagePath = imagePath;
        this.imageUploaded = true;
    }

    /**
     * Checks if it is a stop token (case insensetive)
     *
     * @param token to check.
     * 
     * @return Bool, true if the token is a stop token 
     */
    public Boolean isStopToken(String token) {
        return token.equalsIgnoreCase(eosToken);
    }
}
