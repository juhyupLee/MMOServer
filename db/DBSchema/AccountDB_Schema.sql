--
-- PostgreSQL database dump
--


-- Dumped from database version 18.0 (Debian 18.0-1.pgdg13+3)
-- Dumped by pg_dump version 18.0

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET transaction_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: tAccount; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public."tAccount" (
    "AccountUID" bigint CONSTRAINT "Account_AccountUID_not_null" NOT NULL
);


--
-- Name: Account_AccountUID_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public."tAccount" ALTER COLUMN "AccountUID" ADD GENERATED ALWAYS AS IDENTITY (
    SEQUENCE NAME public."Account_AccountUID_seq"
    START WITH 100
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: tAccount Account_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public."tAccount"
    ADD CONSTRAINT "Account_pkey" PRIMARY KEY ("AccountUID");


--
-- PostgreSQL database dump complete
--


