(function(){var c=" translation",d=" using Google Translate?",e=".",h="Google has automatically translated this page to: ",k="Powered by ",l="Translate",m="Translate everything to ",n="Translate this page to: ",p="Translated to: ",q="Turn off ",r="Turn off for: ",t="View this page in: ",u="var ",v=this;function w(a,x){var f=a.split(e),b=v;f[0]in b||!b.execScript||b.execScript(u+f[0]);for(var g;f.length&&(g=f.shift());)f.length||void 0===x?b[g]?b=b[g]:b=b[g]={}:b[g]=x};var y={0:l,1:"Cancel",2:"Close",3:function(a){return h+a},4:function(a){return p+a},5:"Error: The server could not complete your request. Try again later.",6:"Learn more",7:function(a){return k+a},8:l,9:"Translation in progress",10:function(a){return n+(a+d)},11:function(a){return t+a},12:"Show original",13:"The content of this local file will be sent to Google for translation using a secure connection.",14:"The content of this secure page will be sent to Google for translation using a secure connection.",
15:"The content of this intranet page will be sent to Google for translation using a secure connection.",16:"Select Language",17:function(a){return q+(a+c)},18:function(a){return r+a},19:"Always hide",20:"Original text:",21:"Contribute a better translation",22:"Contribute",23:"Translate all",24:"Restore all",25:"Cancel all",26:"Translate sections to my language",27:function(a){return m+a},28:"Show original languages",29:"Options",30:"Turn off translation for this site",31:null,32:"Show alternative translations",
33:"Click on words above to get alternative translations",34:"Use",35:"Drag with shift key to reorder",36:"Click for alternative translations",37:"Hold down the shift key, click, and drag the words above to reorder.",38:"Thank you for contributing your translation suggestion to Google Translate.",39:"Manage translation for this site",40:"Click a word for alternative translations, or double-click to edit directly",41:"Original text",42:l,43:l,44:"Your correction has been submitted.",45:"Error: The language of the webpage is not supported."};var z=window.google&&google.translate&&google.translate._const;
if(z){var A;a:{for(var B=[],C=["26,0.01,20150908"],D=0;D<C.length;++D){var E=C[D].split(","),F=E[0];if(F){var G=Number(E[1]);if(!(!G||.1<G||0>G)){var H=Number(E[2]),I=new Date,J=1E4*I.getFullYear()+100*(I.getMonth()+1)+I.getDate();!H||H<J||B.push({version:F,ratio:G,a:H})}}}for(var K=0,L=window.location.href.match(/google\.translate\.element\.random=([\d\.]+)/),M=Number(L&&L[1])||Math.random(),D=0;D<B.length;++D){var N=B[D],K=K+N.ratio;if(1<=K)break;if(M<K){A=N.version;break a}}A="28"}var O="/translate_static/js/element/%s/element_main.js".replace("%s",
A);if("0"==A){var P=" translate_static js element %s element_main.js".split(" ");P[P.length-1]="main.js";O=P.join("/").replace("%s",A)}if(z._cjlc)z._cjlc(z._pas+z._pah+O);else{var Q=z._pas+z._pah+O,R=document.createElement("script");R.type="text/javascript";R.charset="UTF-8";R.src=Q;var S=document.getElementsByTagName("head")[0];S||(S=document.body.parentNode.appendChild(document.createElement("head")));S.appendChild(R)}w("google.translate.m",y);w("google.translate.v",A)};})()
