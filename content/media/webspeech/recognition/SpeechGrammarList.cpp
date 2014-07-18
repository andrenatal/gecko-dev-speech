/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechGrammarList.h"

#include "mozilla/dom/SpeechGrammarListBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsXPCOMStrings.h"


namespace mozilla {
  namespace dom {

    NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SpeechGrammarList, mParent)
    NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechGrammarList)
    NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechGrammarList)
    NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechGrammarList)
      NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
      NS_INTERFACE_MAP_ENTRY(nsISupports)
    NS_INTERFACE_MAP_END

    SpeechGrammarList::SpeechGrammarList(nsISupports* aParent)
      : mParent(aParent)
    {
      SetIsDOMBinding();
    }

    SpeechGrammarList::~SpeechGrammarList()
    {
    }

    SpeechGrammarList*
    SpeechGrammarList::Constructor(const GlobalObject& aGlobal,
                                   ErrorResult& aRv)
    {
      return new SpeechGrammarList(aGlobal.GetAsSupports());
    }

    JSObject*
    SpeechGrammarList::WrapObject(JSContext* aCx)
    {
      return SpeechGrammarListBinding::Wrap(aCx, this);
    }

    nsISupports*
    SpeechGrammarList::GetParentObject() const
    {
      return mParent;
    }

    uint32_t
    SpeechGrammarList::Length() const
    {
      return 0;
    }

    already_AddRefed<SpeechGrammar>
    SpeechGrammarList::Item(uint32_t aIndex, ErrorResult& aRv)
    {
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
      return nullptr;
    }

    void
    SpeechGrammarList::AddFromURI(const nsAString& aSrc,
                                  const Optional<float>& aWeight,
                                  ErrorResult& aRv)
    {
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
      return;
    }

    void SpeechGrammarList::AddFromString(const nsAString& aString,
                                     const Optional<float>& aWeight,
                                     ErrorResult& aRv)
    {

        printf("=== SpeechGrammarList::AddFromString Writing grammar \n " );


        // get temp folder
        nsCOMPtr<nsIFile> tmpFile;
        nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpFile));
        if (NS_WARN_IF(NS_FAILED(rv))) {
           aRv.Throw(NS_ERROR_FILE_NOT_FOUND);
           return;
        }

        rv = tmpFile->Append(NS_LITERAL_STRING("grm.jsgf") );
        //NS_ENSURE_SUCCESS_VOID(rv);

        // write the grammar
        FILE* fpgram;

        rv = tmpFile->OpenANSIFileDesc("w", &fpgram);
        //NS_ENSURE_SUCCESS_VOID(rv);
        nsCString mgramdata = NS_ConvertUTF16toUTF8(aString);

        fwrite(mgramdata.get(),sizeof(char) , mgramdata.Length() , fpgram);

        fclose(fpgram);


        nsString aStringPath;
        tmpFile->GetPath(aStringPath);
        mgram = ToNewUTF8String(aStringPath);

        tmpFile = NULL;
        fpgram = NULL;

        printf("=== Grammar written to path %s === \n " , mgram);

        return;
    }

    already_AddRefed<SpeechGrammar>
    SpeechGrammarList::IndexedGetter(uint32_t aIndex, bool& aPresent,
                                     ErrorResult& aRv)
    {
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
      return nullptr;
    }




  } // namespace dom
} // namespace mozilla
