/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"

#include "PocketSphinxSpeechRecognitionService.h"

#include "SpeechRecognition.h"
#include "SpeechRecognitionAlternative.h"
#include "SpeechRecognitionResult.h"
#include "SpeechRecognitionResultList.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"

extern "C"
{
  #include <pocketsphinx/pocketsphinx.h>
  #include <sphinxbase/sphinx_config.h>
  #include <sphinxbase/jsgf.h>

}

namespace mozilla {

  using namespace dom;

  NS_IMPL_ISUPPORTS(PocketSphinxSpeechRecognitionService, nsISpeechRecognitionService, nsIObserver)

  PocketSphinxSpeechRecognitionService::PocketSphinxSpeechRecognitionService()
  {
    printf("==== CONSTRUCTING  PocketSphinxSpeechRecognitionService === \n");


    // FOR B2G PATHS HARDCODED (APPEND /DATA ON THE BEGINING, FOR DESKTOP, ONLY MODELS/ RELATIVE TO ROOT
    config = cmd_ln_init(NULL, ps_args(), TRUE,
               "-hmm", "models/en-us-semi", // acoustic model
//                "-jsgf", mgram , // initial grammar
               "-dict", "models/dict/cmu07a.dic", // point to yours
               NULL);
     if (config == NULL)
       printf("ERROR CREATING PSCONFIG");

     ps = ps_init(config);
     if (ps == NULL)
       printf("ERROR CREATING PSDECODER \n");

     printf("==== CONSTRUCTED  PocketSphinxSpeechRecognitionService === \n");

     mSpeexState = NULL;

  }

  PocketSphinxSpeechRecognitionService::~PocketSphinxSpeechRecognitionService()
  {
    printf("==== Destructing PocketSphinxSpeechRecognitionService === \n");

    if (config) config = NULL;
    if (ps) ps = NULL;
    if (mSpeexState) mSpeexState = NULL;

    printf("==== DESTRUCTED PocketSphinxSpeechRecognitionService === \n");

  }

  // CALL START IN JS FALLS HERE
  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::Initialize(WeakPtr<SpeechRecognition> aSpeechRecognition)
  {
    printf("==== PocketSphinxSpeechRecognitionService::Initialize  === \n");

    if (mSpeexState)
      mSpeexState = NULL;

    mRecognition = aSpeechRecognition;
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC, false);
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC, false);


    // get temp folder
    nsCOMPtr<nsIFile> tmpFile;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      printf("==== NO TEMP FOLDER EXISTS ==== \n");
       return NS_ERROR_FILE_NOT_FOUND;
    }
    rv = tmpFile->Append(NS_LITERAL_STRING("audio.raw") );
    nsString aStringPath;
    tmpFile->GetPath(aStringPath);
    maudio = ToNewUTF8String(aStringPath);


    printf("==== END OF PocketSphinxSpeechRecognitionService::Initialize  === \n ");

    tmpFile = NULL;

    return NS_OK;
  }

  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::ProcessAudioSegment(int32_t aSampleRate, AudioSegment* aAudioSegment)
  {
    if (!mSpeexState) {
        mSpeexState = speex_resampler_init(1,  44100, 16000,  SPEEX_RESAMPLER_QUALITY_MAX  ,  nullptr);
        printf("==== STATE CREATED === ");

        _file = fopen(maudio, "w");

    }


  //  printf("==== RESAMPLING CHUNKS === ");
    aAudioSegment->ResampleChunks(mSpeexState);


    AudioSegment::ChunkIterator iterator(*aAudioSegment);
    while (!iterator.IsEnded()) {
      //printf("==== START ITERATING === ");

      const int16_t* audio_data = static_cast<const int16_t*>(iterator->mChannelData[0]);

      fwrite(audio_data,sizeof(int16_t) , iterator->mDuration , _file);

      //printf("==== END ITERATING === ");
      iterator.Next();
    }


    return NS_OK;
  }

  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::SoundEnd()
  {
    printf("==== SOUNDEND() ==== \n");
    fclose(_file);

    // Declare javascript result events
    nsRefPtr<SpeechEvent> event =
      new SpeechEvent(mRecognition,
                      SpeechRecognition::EVENT_RECOGNITIONSERVICE_FINAL_RESULT);
    SpeechRecognitionResultList* resultList = new SpeechRecognitionResultList(mRecognition);
    SpeechRecognitionResult* result = new SpeechRecognitionResult(mRecognition);
    SpeechRecognitionAlternative* alternative = new SpeechRecognitionAlternative(mRecognition);
    nsString hypoValue;


    printf("==== SOUNDEND() DESTROYING SPEEX STATE ==== \n");
    speex_resampler_destroy(mSpeexState);
    mSpeexState= NULL;





    printf("==== SOUNDEND() DECODING SPEECH. OPENING FILE === \n");
    _file = fopen(maudio, "r");
    printf("==== SOUNDEND() DECODING RAW === \n");
    const char *hyp, *uttid;
    int32 score;
    int _psrv = ps_decode_raw(ps, _file, NULL, -1);
    if (_psrv < 0)
    {
      printf("ERROR ps_decode_raw");
    }
    fclose(_file);

    printf("==== SOUNDEND() GETTING HYP() === \n");
    hyp = ps_get_hyp(ps, &score, &uttid);

    if (hyp == NULL) {
      printf("ERROR hyp()");
      hypoValue.AssignASCII("");
    } else {
      printf("OK hyp(): ");
      printf(hyp);
      hypoValue.AssignASCII(hyp);
    }


    printf("==== RAISING FINAL RESULT EVENT TO JAVASCRIPT ==== \n");
    alternative->mTranscript =   hypoValue;
    alternative->mConfidence = 0.0f;

    result->mItems.AppendElement(alternative);
    resultList->mItems.AppendElement(result);

    event->mRecognitionResultList = resultList;
    NS_DispatchToMainThread(event);



    _file = NULL;

    return NS_OK;
  }


  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::SetGrammarList(WeakPtr<SpeechGrammarList> aSpeechGramarList)
  {
    const char * mgram = NULL;
    if (aSpeechGramarList)
    {
       mgram = aSpeechGramarList->mgram;
       printf("==== Creating grammar. on path  %s  === \n" , mgram);

       // parse the grammar
        jsgf_rule_iter_t *itor;
        jsgf_t * gram = jsgf_parse_file(aSpeechGramarList->mgram, NULL);
        jsgf_rule_t * rule = NULL;
        for (itor = jsgf_rule_iter(gram); itor; itor = jsgf_rule_iter_next(itor)) {
            rule = jsgf_rule_iter_rule(itor);
            if (jsgf_rule_public(rule))
                 break;
        }

        if (rule)
        {
           fsg_model_t * m = jsgf_build_fsg(gram, rule, ps_get_logmath(ps), 6.5);
           ps_set_fsg(ps, "name", m);
           ps_set_search(ps, "name");
        }

    }
    else
    {
      printf("==== aSpeechGramarList is NULL  === \n" );
    }


    return NS_OK;
  }

  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::Abort()
  {
    return NS_OK;
  }

  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
  {

    MOZ_ASSERT(mRecognition->mTestConfig.mFakeRecognitionService,
               "Got request to fake recognition service event, but "
               TEST_PREFERENCE_FAKE_RECOGNITION_SERVICE " is not set");

    if (!strcmp(aTopic, SPEECH_RECOGNITION_TEST_END_TOPIC)) {
      nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
      obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC);
      obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC);

      return NS_OK;
    }

    const nsDependentString eventName = nsDependentString(aData);

    if (eventName.EqualsLiteral("EVENT_RECOGNITIONSERVICE_ERROR")) {
      mRecognition->DispatchError(SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
                                  SpeechRecognitionErrorCode::Network, // TODO different codes?
                                  NS_LITERAL_STRING("RECOGNITIONSERVICE_ERROR test event"));

    } else if (eventName.EqualsLiteral("EVENT_RECOGNITIONSERVICE_FINAL_RESULT")) {
      nsRefPtr<SpeechEvent> event =
        new SpeechEvent(mRecognition,
                        SpeechRecognition::EVENT_RECOGNITIONSERVICE_FINAL_RESULT);

      event->mRecognitionResultList = BuildMockResultList();
      NS_DispatchToMainThread(event);
    }

    return NS_OK;
    }

    SpeechRecognitionResultList* PocketSphinxSpeechRecognitionService::BuildMockResultList()
    {
      SpeechRecognitionResultList* resultList = new SpeechRecognitionResultList(mRecognition);
      SpeechRecognitionResult* result = new SpeechRecognitionResult(mRecognition);
      SpeechRecognitionAlternative* alternative = new SpeechRecognitionAlternative(mRecognition);

      alternative->mTranscript = NS_LITERAL_STRING("Mock final result");
      alternative->mConfidence = 0.0f;

      result->mItems.AppendElement(alternative);
      resultList->mItems.AppendElement(result);

      return resultList;
    }

} // namespace mozilla
