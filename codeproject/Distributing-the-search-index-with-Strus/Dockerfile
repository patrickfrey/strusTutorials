FROM patrickfrey/strus-ub1404-bind:v0_2_6
MAINTAINER patrickpfrey@yahoo.com

RUN apt-get update \
	&& apt-get -y --force-yes install \
	python-tornado python-snappy curl

RUN pip install websocket-client

COPY search_base_html.tpl /home/strus/search_base_html.tpl
COPY search_bm25_html.tpl /home/strus/search_bm25_html.tpl
COPY search_error_html.tpl /home/strus/search_error_html.tpl
COPY strusIR.py /home/strus/strusIR.py
COPY strusQueryServer.py /home/strus/strusQueryServer.py
COPY strusStatisticsServer.py /home/strus/strusStatisticsServer.py
COPY strusStorageServer.py /home/strus/strusStorageServer.py
COPY storage.conf /home/strus/storage.conf
COPY document.ana /home/strus/document.ana

ENV PYTHONPATH /usr/local/lib/strus/python

# Permissions for start in shell:
# -------------------------------
WORKDIR /home/strus
USER root

# Mapped ports:
# -------------
EXPOSE 80

